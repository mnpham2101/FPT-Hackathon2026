#include "ada/v2x_r2_ingest.hpp"

#include <chrono>

#include "ada/r2_mapper.hpp"

namespace ada {
namespace {

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

V2xR2IngestResult ingest_r2_payload(const std::string& payload, std::int64_t received_ms, TrackStore& store, EventLogger& logger) {
    logger.write("r2_rx", payload);
    const auto object = tracked_object_from_r2_json(payload, received_ms);
    if (!object) {
        logger.write("r2_rejected", "{\"reason\":\"invalid_r2_v2x_object\"}");
        return {false, false, "", "invalid_r2_v2x_object"};
    }

    const auto result = store.upsert(*object);
    logger.write(
        "track_transition",
        "{\"id\":\"" + object->id + "\",\"source\":\"" + std::string(to_string(object->source)) +
            "\",\"state\":\"" + std::string(to_string(result.current)) + "\",\"distance\":" +
            std::to_string(object->distance_m) + ",\"position\":{\"x\":" + std::to_string(object->position.x_m) +
            ",\"y\":" + std::to_string(object->position.y_m) + "}}");
    return {true, false, object->id, "accepted"};
}

V2xR2IngestResult ingest_one_r2_udp(UdpR2Receiver& receiver, std::chrono::milliseconds timeout, TrackStore& store, EventLogger& logger) {
    const auto payload = receiver.receive_one(timeout);
    if (!payload) {
        return {false, true, "", "timeout"};
    }
    return ingest_r2_payload(*payload, now_ms(), store, logger);
}

}  // namespace ada
