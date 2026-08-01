#include "contracts/r2_message.hpp"

namespace ada::contracts {

void to_json(nlohmann::json& j, const R2Sender& s) {
  j = nlohmann::json{{"lat", s.lat}, {"lon", s.lon}, {"heading", s.heading}};
  // Nullable F1 field: the key is always present, null when nullopt.
  j["speed"] = s.speed.has_value() ? nlohmann::json(*s.speed) : nlohmann::json(nullptr);
}

void from_json(const nlohmann::json& j, R2Sender& s) {
  // j.at(...) throws on missing keys (all schema-required); unknown extra keys
  // are deliberately ignored for additive evolution.
  j.at("lat").get_to(s.lat);
  j.at("lon").get_to(s.lon);
  j.at("heading").get_to(s.heading);
  s.speed =
      j.at("speed").is_null() ? std::nullopt : std::optional<double>(j.at("speed").get<double>());
}

void to_json(nlohmann::json& j, const R2Position& p) {
  j = nlohmann::json{{"x", p.x}, {"y", p.y}, {"confidence", p.confidence}};
}

void from_json(const nlohmann::json& j, R2Position& p) {
  j.at("x").get_to(p.x);
  j.at("y").get_to(p.y);
  j.at("confidence").get_to(p.confidence);
}

void to_json(nlohmann::json& j, const R2Object& o) {
  j = nlohmann::json{{"objectId", o.objectId},
                     {"timeOfMeasurement", o.timeOfMeasurement},
                     {"distance", o.distance},
                     {"position", o.position},
                     {"speed", o.speed},
                     {"classification", o.classification}};
  // Nullable F6 field: the key is always present, null when nullopt.
  j["confidence"] =
      o.confidence.has_value() ? nlohmann::json(*o.confidence) : nlohmann::json(nullptr);
}

void from_json(const nlohmann::json& j, R2Object& o) {
  j.at("objectId").get_to(o.objectId);
  j.at("timeOfMeasurement").get_to(o.timeOfMeasurement);
  j.at("distance").get_to(o.distance);
  j.at("position").get_to(o.position);
  j.at("speed").get_to(o.speed);
  j.at("classification").get_to(o.classification);
  o.confidence = j.at("confidence").is_null()
                     ? std::nullopt
                     : std::optional<double>(j.at("confidence").get<double>());
}

void to_json(nlohmann::json& j, const R2Message& m) {
  j = nlohmann::json{{"schemaVersion", m.schemaVersion},
                     {"type", m.type},
                     {"stationId", m.stationId},
                     {"rxTime", m.rxTime},
                     {"sender", m.sender},
                     {"object", m.object}};
}

void from_json(const nlohmann::json& j, R2Message& m) {
  j.at("schemaVersion").get_to(m.schemaVersion);
  j.at("type").get_to(m.type);
  j.at("stationId").get_to(m.stationId);
  j.at("rxTime").get_to(m.rxTime);
  j.at("sender").get_to(m.sender);
  j.at("object").get_to(m.object);
}

}  // namespace ada::contracts
