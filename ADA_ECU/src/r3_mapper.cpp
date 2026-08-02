#include "ada/r3_mapper.hpp"

#include <nlohmann/json.hpp>

namespace ada {
namespace {

using Json = nlohmann::json;

std::optional<Source> parse_source(const std::string& value) {
    if (value == "own_sensor") {
        return Source::OwnSensor;
    }
    if (value == "v2x_relayed") {
        return Source::V2xRelayed;
    }
    return std::nullopt;
}

std::optional<TrackState> parse_state(const std::string& value) {
    if (value == "not_tracked") {
        return TrackState::NotTracked;
    }
    if (value == "tentative") {
        return TrackState::Tentative;
    }
    if (value == "tracked") {
        return TrackState::Tracked;
    }
    return std::nullopt;
}

}  // namespace

std::optional<TrackedObject> tracked_object_from_r3_json(const std::string& json) {
    Json message;
    try {
        message = Json::parse(json);
    } catch (const Json::parse_error&) {
        return std::nullopt;
    }

    if (!message.is_object() || !message.contains("id") || !message.contains("source") || !message.contains("position") ||
        !message.contains("timestamps")) {
        return std::nullopt;
    }

    const auto source = parse_source(message.value("source", ""));
    const auto state = parse_state(message.value("state", "not_tracked"));
    if (!source || !state || !message["position"].is_object() || !message["timestamps"].is_object()) {
        return std::nullopt;
    }

    const auto& position = message["position"];
    const auto& timestamps = message["timestamps"];
    if (!position.contains("x") || !position.contains("y")) {
        return std::nullopt;
    }

    try {
        TrackedObject object;
        object.id = message.at("id").get<std::string>();
        object.object_class = message.value("class", "vehicle");
        object.source = *source;
        object.position = {
            position.at("x").get<double>(),
            position.at("y").get<double>(),
            position.value("confidence", 0.0),
        };
        object.distance_m = message.at("distance").get<double>();
        object.speed_mps = message.value("speed", 0.0);
        object.confidence = message.value("confidence", 0.0);
        object.state = *state;
        object.timestamps = {
            timestamps.value("measured", 0LL),
            timestamps.value("received", 0LL),
            timestamps.value("lastUpdated", 0LL),
        };
        return object;
    } catch (const Json::exception&) {
        return std::nullopt;
    }
}

}  // namespace ada

