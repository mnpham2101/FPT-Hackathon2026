#include <cassert>
#include <string>

#include "ada/r2_mapper.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"
#include "ada/warning_builder.hpp"

namespace {

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

    ada::TrackStore store(config);
    const auto first = store.upsert(own_b(1000));
    assert(first.current == ada::TrackState::Tentative);
    const auto second = store.upsert(own_b(1010));
    assert(second.current == ada::TrackState::Tracked);

    const std::string r2 = R"({"schemaVersion":1,"type":"v2x_object","stationId":1201,"rxTime":1789000000123,"sender":{"lat":21.028511,"lon":105.804817,"heading":90.0,"speed":16.7},"object":{"objectId":7,"timeOfMeasurement":-50,"distance":25.4,"position":{"x":25.0,"y":1.2,"confidence":0.9},"speed":15.2,"classification":"vehicle","confidence":0.95}})";
    const auto relayed = ada::tracked_object_from_r2_json(r2, 1020);
    assert(relayed);
    assert(relayed->source == ada::Source::V2xRelayed);
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

