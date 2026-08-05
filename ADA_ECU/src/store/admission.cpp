// R13 admission state machine (13.2.4.2) — pure transition logic only.
// Edge semantics: doc/phase2-4-ada-ecu-admission.puml, design decision D3.

#include "store/admission.hpp"

namespace ada::store {

using contracts::TrackState;

const char* edgeName(AdmissionEdge edge) {
  switch (edge) {
    case AdmissionEdge::gate_enter:
      return "gate_enter";
    case AdmissionEdge::counted:
      return "counted";
    case AdmissionEdge::confirmed:
      return "confirmed";
    case AdmissionEdge::refreshed:
      return "refreshed";
    case AdmissionEdge::out_of_gate:
      return "out_of_gate";
    case AdmissionEdge::gate_exit:
      return "gate_exit";
    case AdmissionEdge::timeout:
      return "timeout";
    case AdmissionEdge::none:
      break;
  }
  return "none";
}

AdmissionMachine::AdmissionMachine(double gateEnterM, double gateExitM, int confirmHits,
                                   std::int64_t trackTimeoutMs)
    : gateEnterM_(gateEnterM),
      gateExitM_(gateExitM),
      confirmHits_(confirmHits),
      trackTimeoutMs_(trackTimeoutMs) {}

AdmissionResult AdmissionMachine::step(const AdmissionInput& in) const {
  // Default: no edge fired, everything as it was.
  AdmissionResult r;
  r.state = in.state;
  r.hits = in.hits;
  r.action = AdmissionAction::none;
  r.edge = AdmissionEdge::none;

  if (in.state == TrackState::not_tracked) {
    // Absent from the store: nothing can be stale, only the admit edge
    // exists. An expiry tick (no distance) or an out-of-gate sighting
    // fires nothing.
    if (in.distanceM.has_value() && *in.distanceM <= gateEnterM_) {
      r.hits = 1;
      r.action = AdmissionAction::insert;
      if (r.hits >= confirmHits_) {
        // confirmHits == 1: the tentative range 1..CONFIRM_HITS−1 is empty.
        r.state = TrackState::tracked;
        r.edge = AdmissionEdge::confirmed;
      } else {
        r.state = TrackState::tentative;
        r.edge = AdmissionEdge::gate_enter;
      }
    }
    return r;
  }

  // Existing entry (tentative or tracked): the timeout edge is evaluated
  // first, on every stimulus kind — see step()'s precedence note.
  if (in.elapsedMs > trackTimeoutMs_) {
    r.state = TrackState::not_tracked;
    r.hits = 0;
    r.action = AdmissionAction::erase;
    r.edge = AdmissionEdge::timeout;
    return r;
  }

  if (!in.distanceM.has_value()) {
    // Expiry tick inside the timeout: no edge.
    return r;
  }
  const double distance = *in.distanceM;

  if (in.state == TrackState::tentative) {
    // The narrow gate both admits and holds while tentative (Schmitt band).
    if (distance > gateEnterM_) {
      r.state = TrackState::not_tracked;
      r.hits = 0;
      r.action = AdmissionAction::erase;
      r.edge = AdmissionEdge::out_of_gate;
      return r;
    }
    r.hits = in.hits + 1;
    r.action = AdmissionAction::keep;
    if (r.hits >= confirmHits_) {
      r.state = TrackState::tracked;
      r.edge = AdmissionEdge::confirmed;
    } else {
      r.edge = AdmissionEdge::counted;
    }
    return r;
  }

  // tracked: only past the wider gate does the entry drop.
  if (distance > gateExitM_) {
    r.state = TrackState::not_tracked;
    r.hits = 0;
    r.action = AdmissionAction::erase;
    r.edge = AdmissionEdge::gate_exit;
    return r;
  }
  r.action = AdmissionAction::keep;
  r.edge = AdmissionEdge::refreshed;
  return r;
}

}  // namespace ada::store
