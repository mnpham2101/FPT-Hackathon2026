#ifndef ADA_ECU_CONTRACTS_R4_MESSAGE_HPP
#define ADA_ECU_CONTRACTS_R4_MESSAGE_HPP

// R4 ADA-to-IVI binding, ADA (producer) side — frozen R4 schema 4.0.1.5
// (R3 $ref 3.0.1.4). Synced schema copy: ADA_ECU/contracts/r4-ada-ivi.schema.json
// oneOf discriminated by "type": warning event ("warning") + state message
// ("state"). The embedded `object` snapshot reuses the R3 TrackedObject binding
// (tracked_object.hpp), which also provides Vec2. Unknown extra keys are
// ignored (additive evolution, HLD D4 — tested by 4.0.4.4).

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "contracts/tracked_object.hpp"

namespace ada::contracts {

// Vehicle-position set shared by BOTH message kinds — the warning's `geometry`
// and the state's `vehicles` have the identical shape.
// vehicleC nullability spans both schema variants:
//   - warning `geometry`: key required, value object-or-null;
//   - state `vehicles`: key absent, null, or object.
// from_json maps missing-key OR null to nullopt (tolerates both variants);
// to_json always emits the "vehicleC" key, null when nullopt — valid wire
// output for both message kinds.
struct R4VehicleSet {
  Vec2 ego;
  Vec2 vehicleB;
  std::optional<Vec2> vehicleC;  // nullopt = absent/null (C not yet tracked)
};

inline bool operator==(const R4VehicleSet& a, const R4VehicleSet& b) {
  return a.ego == b.ego && a.vehicleB == b.vehicleB && a.vehicleC == b.vehicleC;
}

// Edge-triggered warning event (type == "warning"); all keys required.
struct R4WarningEvent {
  int schemaVersion{};
  std::string type;         // const "warning"
  std::string warningType;  // warning-registry key; M1 registry: "nlos_obstruction"
  std::string riskState;    // R14 risk level; M1 NLOS plugin emits low | medium | high
  TrackedObject object;     // R3 snapshot of the triggering track (C); wire name kept
  R4VehicleSet geometry;    // composed scene, frozen to the IVI SceneGeometry model
};

inline bool operator==(const R4WarningEvent& a, const R4WarningEvent& b) {
  return a.schemaVersion == b.schemaVersion && a.type == b.type &&
         a.warningType == b.warningType && a.riskState == b.riskState && a.object == b.object &&
         a.geometry == b.geometry;
}

// Periodic awareness state (type == "state"), last-value-wins by seq (R15).
struct R4StateMessage {
  int schemaVersion{};
  std::string type;      // const "state"
  std::int64_t seq{};    // sequence number, last-value-wins
  R4VehicleSet vehicles;
};

inline bool operator==(const R4StateMessage& a, const R4StateMessage& b) {
  return a.schemaVersion == b.schemaVersion && a.type == b.type && a.seq == b.seq &&
         a.vehicles == b.vehicles;
}

void to_json(nlohmann::json& j, const R4VehicleSet& v);
void from_json(const nlohmann::json& j, R4VehicleSet& v);

void to_json(nlohmann::json& j, const R4WarningEvent& w);
void from_json(const nlohmann::json& j, R4WarningEvent& w);

void to_json(nlohmann::json& j, const R4StateMessage& s);
void from_json(const nlohmann::json& j, R4StateMessage& s);

}  // namespace ada::contracts

#endif  // ADA_ECU_CONTRACTS_R4_MESSAGE_HPP
