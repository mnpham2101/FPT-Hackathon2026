#include <chrono>
#include <iostream>
#include <string>

#include "ada/config.hpp"
#include "ada/event_logger.hpp"
#include "ada/r2_mapper.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"
#include "ada/warning_builder.hpp"

namespace {

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "ada-ecu/config/ada-ecu.conf";
    bool mock = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--mock") {
            mock = true;
        }
    }

    const auto config = ada::load_config(config_path);
    ada::TrackStore store(config);
    ada::EventLogger logger(config.log_path);
    ada::NlosRiskAssessor risk(config.gate_enter_m);

    if (!mock) {
        std::cout << "ADA ECU scaffold ready. Use --mock for a Phase 2 smoke run.\n";
        return 0;
    }

    const auto ts = now_ms();
    ada::TrackedObject own_b;
    own_b.id = "own:B";
    own_b.source = ada::Source::OwnSensor;
    own_b.position = {12.0, 0.2, 0.9};
    own_b.distance_m = 12.0;
    own_b.speed_mps = 15.0;
    own_b.confidence = 0.92;
    own_b.timestamps = {ts, ts, ts};

    const auto own_result = store.upsert(own_b);
    logger.write("track_transition", "{\"id\":\"own:B\",\"state\":\"" + std::string(ada::to_string(own_result.current)) + "\"}");

    const std::string r2 = R"({"schemaVersion":1,"type":"v2x_object","stationId":1201,"rxTime":1789000000123,"sender":{"lat":21.028511,"lon":105.804817,"heading":90.0,"speed":16.7},"object":{"objectId":7,"timeOfMeasurement":-50,"distance":25.4,"position":{"x":25.0,"y":1.2,"confidence":0.9},"speed":15.2,"classification":"vehicle","confidence":0.95}})";
    const auto relayed_c = ada::tracked_object_from_r2_json(r2, ts);
    if (!relayed_c) {
        std::cerr << "mock R2 parse failed\n";
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

