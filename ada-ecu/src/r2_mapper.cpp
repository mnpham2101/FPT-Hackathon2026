#include "ada/r2_mapper.hpp"

#include <regex>

namespace ada {
namespace {

std::optional<double> number_after(const std::string& json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return std::stod(match[1].str());
}

std::optional<int> int_after(const std::string& json, const std::string& key) {
    const auto value = number_after(json, key);
    if (!value) {
        return std::nullopt;
    }
    return static_cast<int>(*value);
}

std::optional<std::string> string_after(const std::string& json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str();
}

std::optional<double> last_number_after(const std::string& json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    auto begin = std::sregex_iterator(json.begin(), json.end(), pattern);
    auto end = std::sregex_iterator();
    if (begin == end) {
        return std::nullopt;
    }

    std::smatch last;
    for (auto it = begin; it != end; ++it) {
        last = *it;
    }
    return std::stod(last[1].str());
}

}  // namespace

std::optional<TrackedObject> tracked_object_from_r2_json(const std::string& json, std::int64_t received_ms) {
    const auto type = string_after(json, "type");
    const auto station_id = int_after(json, "stationId");
    const auto object_start = json.find("\"object\"");
    if (object_start == std::string::npos) {
        return std::nullopt;
    }
    const auto object_json = json.substr(object_start);
    const auto object_id = int_after(object_json, "objectId");
    const auto distance = number_after(object_json, "distance");
    const auto x = number_after(object_json, "x");
    const auto y = number_after(object_json, "y");
    const auto speed = number_after(object_json, "speed");
    const auto position_confidence = number_after(object_json, "confidence");
    const auto object_confidence = last_number_after(object_json, "confidence");

    if (!type || *type != "v2x_object" || !station_id || !object_id || !distance || !x || !y) {
        return std::nullopt;
    }

    TrackedObject object;
    object.id = "v2x:" + std::to_string(*station_id) + ":" + std::to_string(*object_id);
    object.source = Source::V2xRelayed;
    object.position = {*x, *y, position_confidence.value_or(0.0)};
    object.distance_m = *distance;
    object.speed_mps = speed.value_or(0.0);
    object.confidence = object_confidence.value_or(0.0);
    object.timestamps = {received_ms, received_ms, received_ms};
    return object;
}

}  // namespace ada
