#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "ada/detector_jsonl_ingest.hpp"
#include "ada/event_logger.hpp"
#include "ada/r2_mapper.hpp"
#include "ada/r3_mapper.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"
#include "ada/v2x_r2_ingest.hpp"
#include "ada/warning_builder.hpp"

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    assert(in);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}  // namespace

int main() {
    ada::AdaConfig config;
    config.gate_enter_m = 30.0;
    config.gate_exit_m = 35.0;
    config.miss_limit_ms = 1500;
    config.tentative_hits = 2;

    const auto own_sensor_jsonl = read_file(std::string(ADA_TESTDATA_DIR) + "/r3_own_sensor.jsonl");
    const auto first_line_end = own_sensor_jsonl.find('\n');
    const auto first_own = ada::tracked_object_from_r3_json(own_sensor_jsonl.substr(0, first_line_end));
    const auto second_own = ada::tracked_object_from_r3_json(own_sensor_jsonl.substr(first_line_end + 1));
    assert(first_own);
    assert(second_own);
    assert(first_own->source == ada::Source::OwnSensor);
    assert(first_own->id == "own:B");

    ada::TrackStore store(config);
    const auto event_log_path = std::filesystem::temp_directory_path() / "ada_core_tests.jsonl";
    ada::EventLogger logger(event_log_path.string());
    const auto ingest = ada::ingest_own_sensor_jsonl_file(std::string(ADA_TESTDATA_DIR) + "/r3_own_sensor.jsonl", store, logger);
    assert(ingest.accepted == 2);
    assert(ingest.rejected == 0);
    const auto own_track = store.get("own:B");
    assert(own_track);
    assert(own_track->state == ada::TrackState::Tracked);

    const auto r2 = read_file(std::string(ADA_TESTDATA_DIR) + "/r2_v2x_object.sample.json");
    const auto relayed = ada::tracked_object_from_r2_json(r2, 1020);
    assert(relayed);
    assert(relayed->source == ada::Source::V2xRelayed);
    assert(relayed->id == "v2x:1201:7");
    assert(relayed->speed_mps == 15.2);
    assert(relayed->confidence == 0.95);
    const auto r2_ingest = ada::ingest_r2_payload(r2, 1020, store, logger);
    assert(r2_ingest.accepted);
    assert(!r2_ingest.timed_out);
    assert(r2_ingest.track_id == "v2x:1201:7");
    assert(store.get("v2x:1201:7")->state == ada::TrackState::Tracked);

    ada::NlosRiskAssessor risk(30.0, 15.0, 0);
    const auto event = risk.assess(store, 1020);
    assert(event);
    assert(event->state == ada::RiskState::Medium);
    const auto warning = ada::build_r4_warning_json(*event, store);
    assert(warning.find("\"type\":\"warning\"") != std::string::npos);
    assert(warning.find("\"warningType\":\"nlos_obstruction\"") != std::string::npos);
    assert(warning.find("\"source\":\"v2x_relayed\"") != std::string::npos);
    assert(warning.find("\"riskState\":\"medium\"") != std::string::npos);
    assert(warning.find("\"trackedObjects\"") != std::string::npos);
    assert(warning.find("\"vehicleB\"") != std::string::npos);
    assert(warning.find("\"vehicleC\"") != std::string::npos);
    assert(warning.find("\"id\":\"own:B\"") != std::string::npos);
    assert(warning.find("\"distance\":12") != std::string::npos);

    auto still_in_range = *relayed;
    still_in_range.distance_m = 24.0;
    still_in_range.timestamps.last_updated_ms = 1200;
    const auto still_in_range_result = store.upsert(still_in_range);
    assert(still_in_range_result.current == ada::TrackState::Tracked);
    assert(!risk.assess(store, 1200));

    auto outside_gate = *relayed;
    outside_gate.distance_m = 36.0;
    const auto outside_gate_result = store.upsert(outside_gate);
    assert(outside_gate_result.current == ada::TrackState::NotTracked);
    const auto clear_event = risk.assess(store, 1300);
    assert(clear_event);
    assert(clear_event->state == ada::RiskState::Low);
    assert(clear_event->object.distance_m == 36.0);

    const auto readmitted = store.upsert(*relayed);
    assert(readmitted.current == ada::TrackState::Tracked);
    const auto readmitted_event = risk.assess(store, 1400);
    assert(readmitted_event);
    assert(readmitted_event->state == ada::RiskState::Medium);
    auto critical = *relayed;
    critical.distance_m = 10.0;
    critical.timestamps.last_updated_ms = 1450;
    store.upsert(critical);
    const auto critical_event = risk.assess(store, 1450);
    assert(critical_event);
    assert(critical_event->state == ada::RiskState::High);
    const auto expire_at = critical.timestamps.last_updated_ms + config.miss_limit_ms + 1;
    store.expire_source(ada::Source::V2xRelayed, expire_at);
    assert(store.get("v2x:1201:7")->state == ada::TrackState::NotTracked);
    assert(store.get("own:B")->state == ada::TrackState::Tracked);
    const auto timeout_clear_event = risk.assess(store, expire_at);
    assert(timeout_clear_event);
    assert(timeout_clear_event->state == ada::RiskState::Low);
    const auto timeout_clear_warning = ada::build_r4_warning_json(*timeout_clear_event, store);
    assert(timeout_clear_warning.find("\"id\":\"own:B\"") != std::string::npos);
    assert(timeout_clear_warning.find("\"riskState\":\"low\"") != std::string::npos);
    assert(timeout_clear_warning.find("\"vehicleB\":{\"x\":12") != std::string::npos);

    return 0;
}
