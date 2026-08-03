#include "ada/ada_pipeline.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "ada/detector_jsonl_ingest.hpp"
#include "ada/event_logger.hpp"
#include "ada/track_store.hpp"
#include "ada/warning_builder.hpp"

namespace ada {

AdaPipeline::AdaPipeline(
    const AdaConfig& config,
    TrackStore& store,
    CollisionRiskAssessor& risk,
    EventLogger& logger,
    R4Send send_r4)
    : config_(config), store_(store), risk_(risk), logger_(logger), send_r4_(std::move(send_r4)) {
    if (!send_r4_) {
        throw std::runtime_error("ADA pipeline requires an R4 sender");
    }
}

bool AdaPipeline::ingest_own_sensor(const std::string& line) {
    return ingest_own_sensor_jsonl_line(line, store_, logger_);
}

V2xR2IngestResult AdaPipeline::ingest_r2(const std::string& payload, std::int64_t received_ms) {
    return ingest_r2_payload(payload, received_ms, store_, logger_);
}

bool AdaPipeline::assess_and_emit(std::int64_t now_ms) {
    const auto event = risk_.assess(store_, now_ms);
    if (!event) {
        return false;
    }

    const auto warning = build_r4_warning_json(*event, store_);
    nlohmann::json assessment{
        {"timestampMs", now_ms},
        {"plugin", "nlos_obstruction"},
        {"trackId", event->object.id},
        {"distanceAC", event->distance_m},
        {"riskState", to_string(event->state)},
        {"rationale", event->rationale},
        {"ttc", event->ttc_s ? nlohmann::json(*event->ttc_s) : nlohmann::json(nullptr)},
    };
    logger_.write("assessment", assessment.dump());
    logger_.write("risk_transition", nlohmann::json{{"riskState", to_string(event->state)},
                                                    {"trackId", event->object.id}}
                                         .dump());

    bool sent = false;
    try {
        send_r4_(warning);
        sent = true;
    } catch (const std::exception& error) {
        logger_.write("r4_tx_failed", nlohmann::json{{"host", config_.ivi_host},
                                                    {"port", config_.ivi_port},
                                                    {"reason", error.what()}}
                                         .dump());
    }
    logger_.write("r4_tx", nlohmann::json{{"host", config_.ivi_host},
                                         {"port", config_.ivi_port},
                                         {"length", warning.size()},
                                         {"sent", sent},
                                         {"body", nlohmann::json::parse(warning)}}
                              .dump());
    std::cout << warning << "\n";
    return true;
}

}  // namespace ada
