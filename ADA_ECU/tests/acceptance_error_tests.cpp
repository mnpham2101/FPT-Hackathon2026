#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ada/detector_jsonl_ingest.hpp"
#include "ada/event_logger.hpp"
#include "ada/track_store.hpp"
#include "ada/types.hpp"
#include "ada/udp_r4_sender.hpp"
#include "ada/v2x_r2_ingest.hpp"

namespace {

template <typename Fn>
bool throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

ada::TrackedObject object(std::string id, ada::Source source, double distance, std::int64_t updated_ms) {
    ada::TrackedObject result;
    result.id = std::move(id);
    result.object_class = "vehicle";
    result.source = source;
    result.position = {distance, 0.0, 0.9};
    result.distance_m = distance;
    result.confidence = 0.9;
    result.timestamps = {updated_ms, updated_ms, updated_ms};
    return result;
}

}  // namespace

int main() {
    ada::AdaConfig config;
    config.gate_enter_m = 30.0;
    config.gate_exit_m = 35.0;
    config.miss_limit_ms = 1000;
    config.tentative_hits = 3;

    const auto log_path = std::filesystem::temp_directory_path() / "ada_acceptance_error_tests.jsonl";
    std::filesystem::remove(log_path);
    ada::EventLogger logger(log_path.string());
    ada::TrackStore malformed_store(config);
    const auto malformed_r2 = ada::ingest_r2_payload("{broken", 1000, malformed_store, logger);
    assert(!malformed_r2.accepted);
    const auto wrong_type_r2 = ada::ingest_r2_payload(
        R"({"schemaVersion":"one","type":"v2x_object","stationId":1,"object":{}})",
        1001,
        malformed_store,
        logger);
    assert(!wrong_type_r2.accepted);
    assert(!ada::ingest_own_sensor_jsonl_line("[]", malformed_store, logger));
    assert(!ada::ingest_own_sensor_jsonl_line(
        R"({"id":7,"source":"own_sensor","position":{},"timestamps":{}})", malformed_store, logger));
    assert(malformed_store.all().empty());
    const auto rejection_log = read_file(log_path);
    assert(rejection_log.find("r2_rejected") != std::string::npos);
    assert(rejection_log.find("parse_reject") != std::string::npos);

    ada::TrackStore relayed_store(config);
    auto relayed = object("v2x:1201:7", ada::Source::V2xRelayed, 31.0, 1000);
    assert(relayed_store.upsert(relayed).current == ada::TrackState::NotTracked);
    relayed.distance_m = 30.0;
    assert(relayed_store.upsert(relayed).current == ada::TrackState::Tracked);
    relayed.distance_m = 35.0;
    assert(relayed_store.upsert(relayed).current == ada::TrackState::Tracked);
    relayed.distance_m = 35.01;
    assert(relayed_store.upsert(relayed).current == ada::TrackState::NotTracked);
    relayed.distance_m = 34.0;
    assert(relayed_store.upsert(relayed).current == ada::TrackState::NotTracked);
    relayed.distance_m = 30.0;
    relayed.timestamps.last_updated_ms = 1100;
    assert(relayed_store.upsert(relayed).current == ada::TrackState::Tracked);
    assert(relayed_store.expire_source(ada::Source::V2xRelayed, 2100).empty());
    assert(relayed_store.expire_source(ada::Source::V2xRelayed, 2101).size() == 1);
    relayed.timestamps.last_updated_ms = 2200;
    assert(relayed_store.upsert(relayed).current == ada::TrackState::Tracked);

    ada::TrackStore own_store(config);
    auto own = object("own:B", ada::Source::OwnSensor, 12.0, 1000);
    assert(own_store.upsert(own).current == ada::TrackState::Tentative);
    assert(own_store.upsert(own).current == ada::TrackState::Tentative);
    assert(own_store.upsert(own).current == ada::TrackState::Tracked);
    assert(own_store.expire_source(ada::Source::OwnSensor, 2001).size() == 1);
    own.timestamps.last_updated_ms = 2100;
    assert(own_store.upsert(own).current == ada::TrackState::Tentative);
    assert(own_store.upsert(own).current == ada::TrackState::Tentative);
    assert(own_store.upsert(own).current == ada::TrackState::Tracked);

    const ada::UdpR4Sender invalid_host("not-an-ip", 47300);
    assert(throws([&] { invalid_host.send("{}"); }));
    const ada::UdpR4Sender oversized_datagram("127.0.0.1", 47300);
    assert(throws([&] { oversized_datagram.send(std::string(70000, 'x')); }));

    std::filesystem::remove(log_path);
    return 0;
}
