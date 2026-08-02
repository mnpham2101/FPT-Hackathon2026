#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "ada/config.hpp"
#include "ada/detector_jsonl_ingest.hpp"
#include "ada/event_logger.hpp"
#include "ada/risk_assessor.hpp"
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
    ada::NlosRiskAssessor& risk,
    const ada::TrackStore& store,
    ada::EventLogger& logger,
    const ada::UdpR4Sender& r4_sender,
    const ada::AdaConfig& config) {
    const auto risk_event = risk.assess(store);
    if (!risk_event) {
        return;
    }

    const auto warning = ada::build_r4_warning_json(*risk_event, store);
    logger.write("risk_event", warning);
    try {
        r4_sender.send(warning);
        logger.write("r4_tx", "{\"host\":\"" + config.ivi_host + "\",\"port\":" + std::to_string(config.ivi_port) + "}");
    } catch (const std::exception& exc) {
        logger.write(
            "r4_tx_failed",
            "{\"host\":\"" + config.ivi_host + "\",\"port\":" + std::to_string(config.ivi_port) +
                ",\"reason\":\"" + exc.what() + "\"}");
    }
    std::cout << warning << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "ADA_ECU/config/ada-ecu.conf";
    bool mock = false;
    bool listen_once = false;
    bool receive_r2 = false;
    int max_r2 = 1;
    std::string r2_sample_path = "ADA_ECU/testdata/r2_v2x_object.sample.json";
    std::string mock_distances_csv;
    std::int64_t mock_received_ms = -1;
    int mock_start_delay_ms = 0;
    std::string own_sensor_sample_path = "ADA_ECU/testdata/r3_own_sensor.jsonl";

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
    ada::TrackStore store(config);
    ada::EventLogger logger(config.log_path);
    ada::NlosRiskAssessor risk(config.gate_enter_m);
    ada::UdpR4Sender r4_sender(config.ivi_host, config.ivi_port);

    if (!mock && !listen_once && !receive_r2) {
        std::cout << "ADA ECU scaffold ready. Use --listen-once, --max-r2 <n>, or --mock for loopback smoke.\n";
        return 0;
    }

    if (max_r2 <= 0) {
        std::cerr << "--max-r2 must be > 0\n";
        return 5;
    }

    const auto own_sensor_ingest = ada::ingest_own_sensor_jsonl_file(own_sensor_sample_path, store, logger);
    if (own_sensor_ingest.accepted == 0 || own_sensor_ingest.rejected > 0) {
        std::cerr << "own-sensor JSONL ingest failed\n";
        return 4;
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
            emit_risk_event_if_needed(risk, store, logger, r4_sender, config);
        } else {
            int sample_index = 0;
            for (const auto distance : distances) {
                const auto r2_ingest = ada::ingest_r2_payload(
                    r2_with_distance(sample, distance), now + sample_index * 100, store, logger);
                if (!r2_ingest.accepted) {
                    std::cerr << "R2 ingest failed: " << r2_ingest.reason << "\n";
                    return 2;
                }
                emit_risk_event_if_needed(risk, store, logger, r4_sender, config);
                ++sample_index;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(config.miss_limit_ms + config.r2_receive_timeout_ms));
            const auto wall_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            store.expire_source(ada::Source::V2xRelayed, wall_now);
            logger.write("track_expire_tick", "{\"now\":" + std::to_string(wall_now) + "}");
            emit_risk_event_if_needed(risk, store, logger, r4_sender, config);
        }
        return 0;
    }

    ada::UdpR2Receiver receiver(config.ada_listen_host, config.ada_listen_port);
    int processed = 0;
    auto last_expire_check = std::chrono::steady_clock::now();
    while (processed < max_r2) {
        const auto r2_ingest =
            ada::ingest_one_r2_udp(receiver, std::chrono::milliseconds(config.r2_receive_timeout_ms), store, logger);
        if (r2_ingest.timed_out) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_expire_check).count();
            if (elapsed >= config.miss_limit_ms) {
                const auto wall_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
                store.expire_source(ada::Source::V2xRelayed, wall_now);
                logger.write("track_expire_tick", "{\"now\":" + std::to_string(wall_now) + "}");
                emit_risk_event_if_needed(risk, store, logger, r4_sender, config);
                last_expire_check = now;
                if (processed > 0) {
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

        emit_risk_event_if_needed(risk, store, logger, r4_sender, config);
    }

    return 0;
}
