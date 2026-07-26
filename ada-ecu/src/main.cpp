#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "ada/config.hpp"
#include "ada/event_logger.hpp"
#include "ada/r2_mapper.hpp"
#include "ada/r3_mapper.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"
#include "ada/udp_r2_receiver.hpp"
#include "ada/warning_builder.hpp"

namespace {

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool ingest_own_sensor_jsonl(const std::string& path, ada::TrackStore& store, ada::EventLogger& logger) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open own-sensor JSONL: " + path);
    }

    bool ingested = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        logger.write("own_sensor_rx", line);
        const auto object = ada::tracked_object_from_r3_json(line);
        if (!object || object->source != ada::Source::OwnSensor) {
            return false;
        }
        const auto result = store.upsert(*object);
        logger.write("track_transition", "{\"id\":\"" + object->id + "\",\"state\":\"" + std::string(ada::to_string(result.current)) + "\"}");
        ingested = true;
    }
    return ingested;
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "ada-ecu/config/ada-ecu.conf";
    bool mock = false;
    bool listen_once = false;
    std::string r2_sample_path = "ada-ecu/testdata/r2_v2x_object.sample.json";
    std::string own_sensor_sample_path = "ada-ecu/testdata/r3_own_sensor.jsonl";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--mock") {
            mock = true;
        } else if (arg == "--listen-once") {
            listen_once = true;
        } else if (arg == "--r2-sample" && i + 1 < argc) {
            r2_sample_path = argv[++i];
        } else if (arg == "--own-sensor-sample" && i + 1 < argc) {
            own_sensor_sample_path = argv[++i];
        }
    }

    const auto config = ada::load_config(config_path);
    ada::TrackStore store(config);
    ada::EventLogger logger(config.log_path);
    ada::NlosRiskAssessor risk(config.gate_enter_m);

    if (!mock && !listen_once) {
        std::cout << "ADA ECU scaffold ready. Use --listen-once to receive one R2 UDP datagram or --mock for loopback smoke.\n";
        return 0;
    }

    if (!ingest_own_sensor_jsonl(own_sensor_sample_path, store, logger)) {
        std::cerr << "own-sensor JSONL ingest failed\n";
        return 4;
    }

    ada::UdpR2Receiver receiver(config.ada_listen_host, config.ada_listen_port);
    if (mock) {
        const auto sample = read_file(r2_sample_path);
        std::thread sender([&config, sample]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ada::send_udp_datagram("127.0.0.1", config.ada_listen_port, sample);
        });
        sender.detach();
    }

    const auto r2 = receiver.receive_one(std::chrono::milliseconds(2000));
    if (!r2) {
        std::cerr << "timed out waiting for R2 UDP datagram on " << config.ada_listen_host << ":" << config.ada_listen_port << "\n";
        return 3;
    }

    logger.write("r2_rx", *r2);
    const auto relayed_c = ada::tracked_object_from_r2_json(*r2, now_ms());
    if (!relayed_c) {
        std::cerr << "R2 parse failed\n";
        return 2;
    }

    const auto relayed_result = store.upsert(*relayed_c);
    logger.write("track_transition", "{\"id\":\"" + relayed_c->id + "\",\"state\":\"" + std::string(ada::to_string(relayed_result.current)) + "\"}");

    const auto risk_event = risk.assess(store);
    if (risk_event) {
        const auto warning = ada::build_r4_warning_json(*risk_event, store);
        logger.write("risk_event", warning);
        std::cout << warning << "\n";
    }

    return 0;
}
