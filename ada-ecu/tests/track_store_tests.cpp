#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

#include "ada/r2_mapper.hpp"
#include "ada/r3_mapper.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"
#include "ada/warning_builder.hpp"

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    assert(in);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

ada::TrackedObject own_b(std::int64_t ts) {
    ada::TrackedObject object;
    object.id = "own:B";
    object.source = ada::Source::OwnSensor;
    object.distance_m = 12.0;
    object.position = {12.0, 0.0, 0.9};
    object.confidence = 0.9;
    object.timestamps = {ts, ts, ts};
    return object;
}

}  // namespace

int main() {
    ada::AdaConfig config;
    config.gate_enter_m = 30.0;
    config.gate_exit_m = 35.0;
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
    const auto first = store.upsert(*first_own);
    assert(first.current == ada::TrackState::Tentative);
    const auto second = store.upsert(*second_own);
    assert(second.current == ada::TrackState::Tracked);

    const auto r2 = read_file(std::string(ADA_TESTDATA_DIR) + "/r2_v2x_object.sample.json");
    const auto relayed = ada::tracked_object_from_r2_json(r2, 1020);
    assert(relayed);
    assert(relayed->source == ada::Source::V2xRelayed);
    assert(relayed->id == "v2x:1201:7");
    assert(relayed->speed_mps == 15.2);
    assert(relayed->confidence == 0.95);
    const auto relayed_result = store.upsert(*relayed);
    assert(relayed_result.current == ada::TrackState::Tracked);

    ada::NlosRiskAssessor risk(config.gate_enter_m);
    const auto event = risk.assess(store);
    assert(event);
    const auto warning = ada::build_r4_warning_json(*event, store);
    assert(warning.find("\"type\":\"warning\"") != std::string::npos);
    assert(warning.find("\"warningType\":\"nlos_obstruction\"") != std::string::npos);
    assert(warning.find("\"source\":\"v2x_relayed\"") != std::string::npos);

    auto far = *relayed;
    far.distance_m = 36.0;
    const auto far_result = store.upsert(far);
    assert(far_result.current == ada::TrackState::NotTracked);

    return 0;
}
