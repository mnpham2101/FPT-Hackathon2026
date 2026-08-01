#ifndef ADA_ECU_CONTRACTS_R2_MESSAGE_HPP
#define ADA_ECU_CONTRACTS_R2_MESSAGE_HPP

// R2 v2x-object binding, ADA (consumer) side — frozen R2 schema 2.0.1.3
// (F1 nullable sender.speed; F7-derived distance).
// Synced schema copy: ADA_ECU/contracts/r2-v2x-object.schema.json
// Nullable fields (sender.speed, object.confidence) map to std::optional<double>;
// the JSON key is always present — emitted as null when nullopt. Unknown extra
// keys are ignored (additive evolution). Handwritten independently of the
// V2X-side sibling binding; folders couple only through the shared schema/sample.

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace ada::contracts {

// Sender (B) reference state from the CPM.
struct R2Sender {
  double lat{};
  double lon{};
  double heading{};
  std::optional<double> speed;  // m/s; nullopt until derived (F1) — wire null
};

inline bool operator==(const R2Sender& a, const R2Sender& b) {
  return a.lat == b.lat && a.lon == b.lon && a.heading == b.heading && a.speed == b.speed;
}

// Perceived-object position relative to B, metres; confidence is position
// accuracy in metres (F6 — not a probability), so this is not the R3 Vec2.
struct R2Position {
  double x{};
  double y{};
  double confidence{};
};

inline bool operator==(const R2Position& a, const R2Position& b) {
  return a.x == b.x && a.y == b.y && a.confidence == b.confidence;
}

struct R2Object {
  int objectId{};           // 0..65535, B-assigned ID of C
  int timeOfMeasurement{};  // ms vs the CPM referenceTime, -2048..2047
  double distance{};        // B->C range, m — F7-derived hypot(position.x, position.y)
  R2Position position;
  double speed{};
  std::string classification;
  std::optional<double> confidence;  // class confidence 0..1; nullopt when CPM ConfidenceLevel = 101 (F6)
};

inline bool operator==(const R2Object& a, const R2Object& b) {
  return a.objectId == b.objectId && a.timeOfMeasurement == b.timeOfMeasurement &&
         a.distance == b.distance && a.position == b.position && a.speed == b.speed &&
         a.classification == b.classification && a.confidence == b.confidence;
}

struct R2Message {
  int schemaVersion{};
  std::string type;  // const "v2x_object"
  std::uint32_t stationId{};
  std::int64_t rxTime{};  // ms epoch, receive timestamp at the V2X ECU
  R2Sender sender;
  R2Object object;
};

inline bool operator==(const R2Message& a, const R2Message& b) {
  return a.schemaVersion == b.schemaVersion && a.type == b.type && a.stationId == b.stationId &&
         a.rxTime == b.rxTime && a.sender == b.sender && a.object == b.object;
}

void to_json(nlohmann::json& j, const R2Sender& s);
void from_json(const nlohmann::json& j, R2Sender& s);

void to_json(nlohmann::json& j, const R2Position& p);
void from_json(const nlohmann::json& j, R2Position& p);

void to_json(nlohmann::json& j, const R2Object& o);
void from_json(const nlohmann::json& j, R2Object& o);

void to_json(nlohmann::json& j, const R2Message& m);
void from_json(const nlohmann::json& j, R2Message& m);

}  // namespace ada::contracts

#endif  // ADA_ECU_CONTRACTS_R2_MESSAGE_HPP
