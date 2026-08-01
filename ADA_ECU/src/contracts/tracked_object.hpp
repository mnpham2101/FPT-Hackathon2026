#ifndef ADA_ECU_CONTRACTS_TRACKED_OBJECT_HPP
#define ADA_ECU_CONTRACTS_TRACKED_OBJECT_HPP

// R3 TrackedObject binding — frozen R3 schema (subtask 3.0.1.4).
// Synced schema copy: ADA_ECU/contracts/r3-tracked-object.schema.json
// JSON key "class" maps to member `object_class` (C++ keyword) via hand-written
// to_json/from_json; unknown extra keys are ignored (additive evolution).

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace ada::contracts {

enum class Source { own_sensor, v2x_relayed };

NLOHMANN_JSON_SERIALIZE_ENUM(Source, {
    {Source::own_sensor, "own_sensor"},
    {Source::v2x_relayed, "v2x_relayed"},
})

enum class TrackState { not_tracked, tentative, tracked };

NLOHMANN_JSON_SERIALIZE_ENUM(TrackState, {
    {TrackState::not_tracked, "not_tracked"},
    {TrackState::tentative, "tentative"},
    {TrackState::tracked, "tracked"},
})

// Reusable 2D position, ego frame, metres (R4 geometry reuses it — 4.0.4.3).
struct Vec2 {
  double x{};
  double y{};
};

inline bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }

// Milliseconds since epoch.
struct Timestamps {
  std::int64_t measured{};
  std::int64_t received{};
  std::int64_t lastUpdated{};
};

inline bool operator==(const Timestamps& a, const Timestamps& b) {
  return a.measured == b.measured && a.received == b.received && a.lastUpdated == b.lastUpdated;
}

struct TrackedObject {
  std::string id;
  std::string object_class;  // JSON key "class"
  Source source{};
  Vec2 position;
  double distance{};
  double speed{};
  double confidence{};
  TrackState state{};
  Timestamps timestamps;
};

inline bool operator==(const TrackedObject& a, const TrackedObject& b) {
  return a.id == b.id && a.object_class == b.object_class && a.source == b.source &&
         a.position == b.position && a.distance == b.distance && a.speed == b.speed &&
         a.confidence == b.confidence && a.state == b.state && a.timestamps == b.timestamps;
}

void to_json(nlohmann::json& j, const Vec2& v);
void from_json(const nlohmann::json& j, Vec2& v);

void to_json(nlohmann::json& j, const Timestamps& t);
void from_json(const nlohmann::json& j, Timestamps& t);

void to_json(nlohmann::json& j, const TrackedObject& o);
void from_json(const nlohmann::json& j, TrackedObject& o);

}  // namespace ada::contracts

#endif  // ADA_ECU_CONTRACTS_TRACKED_OBJECT_HPP
