#include "contracts/tracked_object.hpp"

namespace ada::contracts {

void to_json(nlohmann::json& j, const Vec2& v) { j = nlohmann::json{{"x", v.x}, {"y", v.y}}; }

void from_json(const nlohmann::json& j, Vec2& v) {
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
}

void to_json(nlohmann::json& j, const Timestamps& t) {
  j = nlohmann::json{
      {"measured", t.measured}, {"received", t.received}, {"lastUpdated", t.lastUpdated}};
}

void from_json(const nlohmann::json& j, Timestamps& t) {
  j.at("measured").get_to(t.measured);
  j.at("received").get_to(t.received);
  j.at("lastUpdated").get_to(t.lastUpdated);
}

void to_json(nlohmann::json& j, const TrackedObject& o) {
  j = nlohmann::json{{"id", o.id},
                     {"class", o.object_class},
                     {"source", o.source},
                     {"position", o.position},
                     {"distance", o.distance},
                     {"speed", o.speed},
                     {"confidence", o.confidence},
                     {"state", o.state},
                     {"timestamps", o.timestamps}};
}

void from_json(const nlohmann::json& j, TrackedObject& o) {
  // j.at(...) throws on missing keys (all schema-required); unknown extra keys
  // are deliberately ignored for additive evolution.
  j.at("id").get_to(o.id);
  j.at("class").get_to(o.object_class);
  j.at("source").get_to(o.source);
  j.at("position").get_to(o.position);
  j.at("distance").get_to(o.distance);
  j.at("speed").get_to(o.speed);
  j.at("confidence").get_to(o.confidence);
  j.at("state").get_to(o.state);
  j.at("timestamps").get_to(o.timestamps);
}

}  // namespace ada::contracts
