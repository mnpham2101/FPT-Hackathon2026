#include <chrono>
#include <filesystem>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "ada/config.hpp"
#include "ada/detector_jsonl_ingest.hpp"
#include "ada/detector_process.hpp"
#include "ada/event_logger.hpp"
#include "ada/r3_mapper.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/risk_registry.hpp"
#include "ada/track_store.hpp"
#include "ada/udp_r2_receiver.hpp"
#include "ada/udp_r4_sender.hpp"
#include "ada/v2x_r2_ingest.hpp"
#include "ada/warning_builder.hpp"

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<double> parse_distances(const std::string& csv) {
    std::vector<double> distances;
    std::stringstream stream(csv);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            distances.push_back(std::stod(item));
        }
    }
    return distances;
}

std::string r2_with_distance(const std::string& sample, double distance) {
    auto json = nlohmann::json::parse(sample);
    json["object"]["distance"] = distance;
    json["object"]["position"]["x"] = distance;
    return json.dump();
}

void emit_risk_event_if_needed(
    ada::CollisionRiskAssessor& risk,
    const ada::TrackStore& store,
    ada::EventLogger& logger,
    const ada::UdpR4Sender& r4_sender,
    const ada::AdaConfig& config) {
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    const auto risk_event = risk.assess(store, now_ms);
    if (!risk_event) {
        return;
    }

    const auto warning = ada::build_r4_warning_json(*risk_event, store);
    nlohmann::json assessment{
        {"timestampMs", now_ms},
        {"plugin", "nlos_obstruction"},
        {"trackId", risk_event->object.id},
        {"distanceAC", risk_event->distance_m},
        {"riskState", ada::to_string(risk_event->state)},
        {"rationale", risk_event->rationale},
    };
    assessment["ttc"] = risk_event->ttc_s ? nlohmann::json(*risk_event->ttc_s) : nlohmann::json(nullptr);
    logger.write("assessment", assessment.dump());
    logger.write(
        "risk_transition",
        "{\"riskState\":\"" + std::string(ada::to_string(risk_event->state)) + "\",\"trackId\":\"" +
            risk_event->object.id + "\"}");
    bool sent = false;
    try {
        r4_sender.send(warning);
        sent = true;
    } catch (const std::exception& exc) {
        logger.write(
            "r4_tx_failed",
            "{\"host\":\"" + config.ivi_host + "\",\"port\":" + std::to_string(config.ivi_port) +
                ",\"reason\":\"" + exc.what() + "\"}");
    }
    logger.write(
        "r4_tx",
        "{\"host\":\"" + config.ivi_host + "\",\"port\":" + std::to_string(config.ivi_port) +
            ",\"length\":" + std::to_string(warning.size()) + ",\"sent\":" + (sent ? "true" : "false") +
            ",\"body\":" + warning + "}");
    std::cout << warning << "\n";
}

void settle_mock_dwell(
    ada::CollisionRiskAssessor& risk,
    const ada::TrackStore& store,
    ada::EventLogger& logger,
    const ada::UdpR4Sender& r4_sender,
    const ada::AdaConfig& config) {
    if (config.risk_dwell_ms <= 0) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(config.risk_dwell_ms));
    emit_risk_event_if_needed(risk, store, logger, r4_sender, config);
}

int drain_detector(ada::DetectorProcess& detector, ada::TrackStore& store, ada::EventLogger& logger) {
    int accepted = 0;
    while (const auto line = detector.poll_line()) {
        if (ada::ingest_own_sensor_jsonl_line(*line, store, logger)) {
            ++accepted;
        }
    }
    return accepted;
}

void log_expired_tracks(const std::vector<ada::TrackedObject>& expired, ada::EventLogger& logger) {
    for (const auto& object : expired) {
        logger.write(
            "track_transition",
            "{\"id\":\"" + object.id + "\",\"source\":\"" + ada::to_string(object.source) +
                "\",\"previous\":\"tracked\",\"state\":\"not_tracked\",\"changed\":true,\"object\":" +
                ada::tracked_object_to_r3_json(object) + "}");
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "ADA_ECU/config/ada-ecu.conf";
    bool mock = false;
    bool listen_once = false;
    bool receive_r2 = false;
    int max_r2 = -1;
    std::string r2_sample_path;
    std::string mock_distances_csv;
    std::int64_t mock_received_ms = -1;
    int mock_start_delay_ms = 0;
    std::string own_sensor_sample_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--mock") {
            mock = true;
        } else if (arg == "--listen-once") {
            listen_once = true;
            receive_r2 = true;
            max_r2 = 1;
        } else if (arg == "--r2-sample" && i + 1 < argc) {
            r2_sample_path = argv[++i];
        } else if (arg == "--mock-distances" && i + 1 < argc) {
            mock = true;
            receive_r2 = true;
            mock_distances_csv = argv[++i];
        } else if (arg == "--mock-received-ms" && i + 1 < argc) {
            mock_received_ms = std::stoll(argv[++i]);
        } else if (arg == "--mock-start-delay-ms" && i + 1 < argc) {
            mock_start_delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--own-sensor-sample" && i + 1 < argc) {
            own_sensor_sample_path = argv[++i];
        } else if (arg == "--max-r2" && i + 1 < argc) {
            max_r2 = std::stoi(argv[++i]);
            receive_r2 = true;
        }
    }

    const auto config = ada::load_config(config_path);
    const auto install_root = std::filesystem::path(config_path).parent_path().parent_path();
    if (r2_sample_path.empty()) {
        r2_sample_path = (install_root / "testdata/r2_v2x_object.sample.json").string();
    }
    if (mock && own_sensor_sample_path.empty()) {
        own_sensor_sample_path = (install_root / "testdata/r3_own_sensor.jsonl").string();
    }
    if (config.detector_enabled) {
        receive_r2 = true;
    }
    ada::TrackStore store(config);
    ada::EventLogger logger(config.log_path);
    auto risk = ada::make_builtin_assessor(config.cra_enabled, config);
    ada::UdpR4Sender r4_sender(config.ivi_host, config.ivi_port);

    if (!mock && !listen_once && !receive_r2 && own_sensor_sample_path.empty()) {
        std::cout << "ADA ECU scaffold ready. Use --listen-once, --max-r2 <n>, or --mock for loopback smoke.\n";
        return 0;
    }

    if (max_r2 == 0) {
        std::cerr << "--max-r2 must not be 0\n";
        return 5;
    }

    if (!own_sensor_sample_path.empty()) {
        const auto own_sensor_ingest = ada::ingest_own_sensor_jsonl_file(own_sensor_sample_path, store, logger);
        if (own_sensor_ingest.accepted == 0 || own_sensor_ingest.rejected > 0) {
            std::cerr << "own-sensor JSONL ingest failed\n";
            return 4;
        }
    }

    if (mock) {
        if (mock_start_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mock_start_delay_ms));
        }
        const auto sample = read_file(r2_sample_path);
        const auto distances = mock_distances_csv.empty() ? std::vector<double>{} : parse_distances(mock_distances_csv);
        if (!distances.empty()) {
            max_r2 = static_cast<int>(distances.size());
        }

        const auto wall_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        const auto now = mock_received_ms >= 0 ? mock_received_ms : wall_now;
        if (distances.empty()) {
            const auto r2_ingest = ada::ingest_r2_payload(sample, now, store, logger);
            if (!r2_ingest.accepted) {
                std::cerr << "R2 ingest failed: " << r2_ingest.reason << "\n";
                return 2;
            }
            emit_risk_event_if_needed(*risk, store, logger, r4_sender, config);
            settle_mock_dwell(*risk, store, logger, r4_sender, config);
        } else {
            int sample_index = 0;
            for (const auto distance : distances) {
                const auto r2_ingest = ada::ingest_r2_payload(
                    r2_with_distance(sample, distance), now + sample_index * 100, store, logger);
                if (!r2_ingest.accepted) {
                    std::cerr << "R2 ingest failed: " << r2_ingest.reason << "\n";
                    return 2;
                }
                emit_risk_event_if_needed(*risk, store, logger, r4_sender, config);
                settle_mock_dwell(*risk, store, logger, r4_sender, config);
                ++sample_index;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(config.miss_limit_ms + config.r2_receive_timeout_ms));
            const auto wall_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            const auto expired = store.expire_source(ada::Source::V2xRelayed, wall_now);
            log_expired_tracks(expired, logger);
            logger.write("track_expire_tick", "{\"now\":" + std::to_string(wall_now) + "}");
            emit_risk_event_if_needed(*risk, store, logger, r4_sender, config);
        }
        return 0;
    }

    std::unique_ptr<ada::DetectorProcess> detector;
    int detector_restarts = 0;
    if (config.detector_enabled) {
        if (config.detector_cmd.empty()) {
            std::cerr << "DETECTOR_ENABLED requires DETECTOR_CMD\n";
            return 6;
        }
        detector = std::make_unique<ada::DetectorProcess>(config.detector_cmd);
        logger.write("detector_spawn", "{\"command\":\"configured\"}");
        detector->start();
    }

    ada::UdpR2Receiver receiver(config.ada_listen_host, config.ada_listen_port);
    int processed = 0;
    auto last_expire_check = std::chrono::steady_clock::now();
    auto last_r2_receive = last_expire_check;
    while (max_r2 < 0 || processed < max_r2) {
        if (detector) {
            drain_detector(*detector, store, logger);
            if (detector->finished()) {
                const auto exit_code = detector->exit_code();
                logger.write("detector_eof", "{\"exitCode\":" + std::to_string(exit_code) + "}");
                detector.reset();
                if (detector_restarts < config.detector_restart_max) {
                    ++detector_restarts;
                    logger.write("detector_restart", "{\"attempt\":" + std::to_string(detector_restarts) + "}");
                    detector = std::make_unique<ada::DetectorProcess>(config.detector_cmd);
                    detector->start();
                }
            }
        }
        const auto r2_ingest =
            ada::ingest_one_r2_udp(receiver, std::chrono::milliseconds(config.r2_receive_timeout_ms), store, logger);
        if (r2_ingest.timed_out) {
            if (detector) {
                drain_detector(*detector, store, logger);
            }
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_expire_check).count();
            if (elapsed >= config.r2_receive_timeout_ms) {
                const auto wall_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
                const auto expired = store.expire(wall_now);
                log_expired_tracks(expired, logger);
                logger.write("track_expire_tick", "{\"now\":" + std::to_string(wall_now) + "}");
                emit_risk_event_if_needed(*risk, store, logger, r4_sender, config);
                last_expire_check = now;
                const auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_r2_receive).count();
                if (max_r2 > 0 && processed > 0 && idle_ms >= config.miss_limit_ms) {
                    return 0;
                }
            }
            continue;
        }
        if (!r2_ingest.accepted) {
            std::cerr << "R2 ingest failed: " << r2_ingest.reason << "\n";
            return 2;
        }
        ++processed;
        last_r2_receive = std::chrono::steady_clock::now();

        emit_risk_event_if_needed(*risk, store, logger, r4_sender, config);
    }

    return 0;
}
