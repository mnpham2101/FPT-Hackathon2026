#ifndef V2X_ECU_CONTRACTS_R2_MESSAGE_HPP
#define V2X_ECU_CONTRACTS_R2_MESSAGE_HPP

// R2 v2x-object binding, V2X (producer) side — frozen R2 schema 2.0.1.3.
// This node emits R2: the R9 relay path fills these structs from a decoded CPM
// and serialises them onto the wire for the ADA ECU to consume.
// Synced schema copy: V2X_ECU/contracts/r2-v2x-object.schema.json
// Nullable fields (sender.speed, object.confidence) map to std::optional<double>;
// the JSON key is always emitted, as null when nullopt — an emitter must never
// omit it. Unknown extra keys are ignored on parse (additive evolution).
// Pure model code: no transport, no codec, no derivation — the F1 speed and the
// F7 distance are computed upstream and assigned here already-derived.
// Handwritten independently of the ADA-side sibling binding; the two node
// folders couple only through the shared schema/sample, never by source import.

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace v2x::contracts {

// Sender (B) reference state, taken from the CPM management/station container.
struct R2Sender {
  double lat{};
  double lon{};
  double heading{};
  std::optional<double> speed;  // m/s; nullopt until derived (F1) — emitted as null
};

inline bool operator==(const R2Sender& a, const R2Sender& b) {
  return a.lat == b.lat && a.lon == b.lon && a.heading == b.heading && a.speed == b.speed;
}

// Perceived-object position relative to B, metres; confidence is position
// accuracy in metres (F6 — not a probability).
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
  double distance{};        // B->C range, m — F7-derived hypot(position.x, position.y), never on the CPM wire
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

}  // namespace v2x::contracts

#endif  // V2X_ECU_CONTRACTS_R2_MESSAGE_HPP
