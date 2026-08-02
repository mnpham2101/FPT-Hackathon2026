#include "ada/r2_mapper.hpp"

#include <nlohmann/json.hpp>

namespace ada {
namespace {

using Json = nlohmann::json;

}  // namespace

std::optional<TrackedObject> tracked_object_from_r2_json(const std::string& json, std::int64_t received_ms) {
    Json message;
    try {
        message = Json::parse(json);
    } catch (const Json::parse_error&) {
        return std::nullopt;
    }

    if (!message.is_object() || message.value("schemaVersion", 0) != 1 || message.value("type", "") != "v2x_object" ||
        !message.contains("stationId") || !message.contains("object") || !message["object"].is_object()) {
        return std::nullopt;
    }

    const auto& r2_object = message["object"];
    if (!r2_object.contains("objectId") || !r2_object.contains("distance") || !r2_object.contains("position") ||
        !r2_object["position"].is_object()) {
        return std::nullopt;
    }

    const auto& position = r2_object["position"];
    if (!position.contains("x") || !position.contains("y")) {
        return std::nullopt;
    }

    try {
        TrackedObject object;
        object.id = "v2x:" + std::to_string(message.at("stationId").get<int>()) + ":" +
                    std::to_string(r2_object.at("objectId").get<int>());
        object.object_class = r2_object.value("classification", "vehicle");
        object.source = Source::V2xRelayed;
        object.position = {
            position.at("x").get<double>(),
            position.at("y").get<double>(),
            position.value("confidence", 0.0),
        };
        object.distance_m = r2_object.at("distance").get<double>();
        object.speed_mps = r2_object.value("speed", 0.0);
        object.confidence = r2_object.value("confidence", 0.0);
        object.timestamps = {received_ms, message.value("rxTime", received_ms), received_ms};
        return object;
    } catch (const Json::exception&) {
        return std::nullopt;
    }
}

}  // namespace ada
