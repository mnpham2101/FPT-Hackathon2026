#ifndef ADA_ECU_STORE_ADMISSION_HPP
#define ADA_ECU_STORE_ADMISSION_HPP

// R13 admission state machine (HLD §6, Business logic, the store/admission
// row; design decision D3): ONE machine for both perception sources,
// realizing doc/phase2-4-ada-ecu-admission.puml edge for edge.
//
// Pure: step() is a function of (current state, hits, distance, elapsed) →
// (next state, action). No I/O, no store access, no clock read inside —
// `elapsedMs` is now − lastUpdated, measured by the caller on
// CLOCK_MONOTONIC (D10) before the stimulus is applied. The caller (the
// track store, wired by a later subtask) applies the returned action to its
// entries and writes one track_transition [EVT] line per fired edge.
//
// Hysteresis is one Schmitt band (D3): GATE_ENTER_M admits and holds while
// tentative; only once tracked does the wider GATE_EXIT_M hold. The hits
// counter resets to 0 on every entry to not_tracked.

#include <cstdint>
#include <optional>

#include "contracts/tracked_object.hpp"

namespace ada::store {

// One stimulus for one track id. Two stimulus kinds share the struct:
//  - an update: distanceM holds the observation's distance (for v2x_relayed
//    one R2 message's object.distance, for own_sensor one detector JSONL
//    line's distance — D3's definition of "update");
//  - an expiry tick: distanceM is nullopt, only the timeout edge can fire.
// For state == not_tracked (absent from the store) pass hits = 0 and
// elapsedMs = 0 — there is no entry to be stale.
struct AdmissionInput {
  contracts::TrackState state = contracts::TrackState::not_tracked;
  int hits = 0;
  std::optional<double> distanceM;  // nullopt = expiry tick, no observation
  std::int64_t elapsedMs = 0;       // now − lastUpdated, monotonic domain
};

// What the store must do with its entry for this track id.
enum class AdmissionAction {
  none,    // no edge fired — the entry (or its absence) is untouched
  insert,  // create the entry with the result's state and hits
  keep,    // entry stays; write back the result's state and hits
  erase,   // remove the entry — not_tracked is absence, never a flag (D3)
};

// The fired edge of the .puml diagram. Names are the caller's
// track_transition `reason` vocabulary; `counted` and `refreshed` are the
// two self-loops, which change no state. none = no edge fired (an
// out-of-gate sighting while not_tracked, or a tick inside the timeout).
enum class AdmissionEdge {
  none,
  gate_enter,   // not_tracked → tentative, distance ≤ GATE_ENTER_M
  counted,      // tentative → tentative, in-gate hit below CONFIRM_HITS
  confirmed,    // tentative → tracked, hits ≥ CONFIRM_HITS
  refreshed,    // tracked → tracked, distance ≤ GATE_EXIT_M
  out_of_gate,  // tentative → not_tracked, distance > GATE_ENTER_M
  gate_exit,    // tracked → not_tracked, distance > GATE_EXIT_M
  timeout,      // tentative|tracked → not_tracked, elapsed > timeout
};

// Stable snake_case name of an edge, for the [EVT] stream's reason field.
const char* edgeName(AdmissionEdge edge);

struct AdmissionResult {
  contracts::TrackState state = contracts::TrackState::not_tracked;
  int hits = 0;
  AdmissionAction action = AdmissionAction::none;
  AdmissionEdge edge = AdmissionEdge::none;
};

// Thresholds are constructor parameters, taken from the loaded Config
// (config/config validates 0 < GATE_ENTER_M < GATE_EXIT_M, CONFIRM_HITS ≥ 1,
// TRACK_TIMEOUT_MS > 0 — this class re-validates nothing). Types match the
// Config fields: gateEnterM/gateExitM double, confirmHits int,
// trackTimeoutMs std::int64_t.
class AdmissionMachine {
 public:
  AdmissionMachine(double gateEnterM, double gateExitM, int confirmHits,
                   std::int64_t trackTimeoutMs);

  // Applies exactly one stimulus. Deterministic precedence when both drop
  // conditions hold: the timeout is evaluated first — an update arriving
  // after silence longer than TRACK_TIMEOUT_MS finds a track the fusion tick
  // would already have erased, so it expires the track rather than holding
  // it (the next update re-admits from not_tracked).
  //
  // With confirmHits == 1 an admitting update creates the entry already
  // tracked (action insert, edge confirmed): the diagram's tentative range
  // 1..CONFIRM_HITS−1 is empty, so tentative is transient.
  AdmissionResult step(const AdmissionInput& in) const;

 private:
  const double gateEnterM_;
  const double gateExitM_;
  const int confirmHits_;
  const std::int64_t trackTimeoutMs_;
};

}  // namespace ada::store

#endif  // ADA_ECU_STORE_ADMISSION_HPP
