#include <cassert>
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
    ada::EventLogger logger("/private/tmp/ada_core_tests.jsonl");
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

    ada::NlosRiskAssessor risk(config.gate_enter_m);
    const auto event = risk.assess(store);
    assert(event);
    const auto warning = ada::build_r4_warning_json(*event, store);
    assert(warning.find("\"type\":\"warning\"") != std::string::npos);
    assert(warning.find("\"warningType\":\"nlos_obstruction\"") != std::string::npos);
    assert(warning.find("\"source\":\"v2x_relayed\"") != std::string::npos);

    auto hysteresis = *relayed;
    hysteresis.distance_m = 32.0;
    hysteresis.timestamps.last_updated_ms = 1200;
    const auto hysteresis_result = store.upsert(hysteresis);
    assert(hysteresis_result.current == ada::TrackState::Tracked);

    auto outside_gate = *relayed;
    outside_gate.distance_m = 36.0;
    const auto outside_gate_result = store.upsert(outside_gate);
    assert(outside_gate_result.current == ada::TrackState::NotTracked);

    const auto readmitted = store.upsert(*relayed);
    assert(readmitted.current == ada::TrackState::Tracked);
    store.expire(1020 + config.miss_limit_ms + 1);
    assert(store.get("v2x:1201:7")->state == ada::TrackState::NotTracked);

    return 0;
}
