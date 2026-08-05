// Tests for the R13 admission state machine (13.2.4.2) — every edge of
// doc/phase2-4-ada-ecu-admission.puml has at least one covering case.
// The machine is pure, so tests drive it with literal stimuli and injected
// elapsed times: deterministic, no clock, no sleeps.
//
// Thresholds mirror the HLD §6 defaults (GATE_ENTER_M 30, GATE_EXIT_M 35,
// CONFIRM_HITS 3, TRACK_TIMEOUT_MS 1000) but are plain test constants —
// the machine takes them as constructor parameters, exactly as the store
// will pass them from the loaded Config.

#include "store/admission.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace {

using ada::contracts::TrackState;
using ada::store::AdmissionAction;
using ada::store::AdmissionEdge;
using ada::store::AdmissionInput;
using ada::store::AdmissionMachine;
using ada::store::AdmissionResult;
using ada::store::edgeName;

constexpr double kGateEnterM = 30.0;
constexpr double kGateExitM = 35.0;
constexpr int kConfirmHits = 3;
constexpr std::int64_t kTimeoutMs = 1000;

AdmissionMachine machine() {
  return AdmissionMachine(kGateEnterM, kGateExitM, kConfirmHits, kTimeoutMs);
}

// Carries state and hits across steps, as the store will for one track id.
struct Sim {
  TrackState state = TrackState::not_tracked;
  int hits = 0;

  AdmissionResult update(const AdmissionMachine& m, double distanceM,
                         std::int64_t elapsedMs = 0) {
    const AdmissionResult r = m.step(AdmissionInput{state, hits, distanceM, elapsedMs});
    state = r.state;
    hits = r.hits;
    return r;
  }

  AdmissionResult tick(const AdmissionMachine& m, std::int64_t elapsedMs) {
    const AdmissionResult r = m.step(AdmissionInput{state, hits, std::nullopt, elapsedMs});
    state = r.state;
    hits = r.hits;
    return r;
  }
};

// --- The admit edge ---------------------------------------------------------

TEST(Admission, AdmitsAtGateEnterInclusive) {
  const AdmissionMachine m = machine();
  Sim sim;
  const AdmissionResult r = sim.update(m, kGateEnterM);  // exactly 30.0
  EXPECT_EQ(r.state, TrackState::tentative);
  EXPECT_EQ(r.hits, 1);
  EXPECT_EQ(r.action, AdmissionAction::insert);
  EXPECT_EQ(r.edge, AdmissionEdge::gate_enter);
}

TEST(Admission, IgnoresSightingBeyondGateEnterWhileNotTracked) {
  const AdmissionMachine m = machine();
  Sim sim;
  // 30.1 is beyond the admit gate; 32.0 is inside the exit band but the wider
  // gate holds only once tracked — neither creates an entry (Schmitt band).
  for (const double d : {30.1, 32.0, 100.0}) {
    const AdmissionResult r = sim.update(m, d);
    EXPECT_EQ(r.state, TrackState::not_tracked) << "distance " << d;
    EXPECT_EQ(r.hits, 0);
    EXPECT_EQ(r.action, AdmissionAction::none);
    EXPECT_EQ(r.edge, AdmissionEdge::none);
  }
}

// --- The full own-sensor lifecycle, both directions -------------------------

TEST(Admission, FullLifecycleUpAndDown) {
  const AdmissionMachine m = machine();
  Sim sim;

  // Up: admit, count, promote on the CONFIRM_HITS-th in-gate update.
  EXPECT_EQ(sim.update(m, 25.0).edge, AdmissionEdge::gate_enter);  // hits 1
  EXPECT_EQ(sim.update(m, 24.0).edge, AdmissionEdge::counted);     // hits 2
  const AdmissionResult promoted = sim.update(m, 23.0);            // hits 3
  EXPECT_EQ(promoted.state, TrackState::tracked);
  EXPECT_EQ(promoted.hits, kConfirmHits);
  EXPECT_EQ(promoted.action, AdmissionAction::keep);
  EXPECT_EQ(promoted.edge, AdmissionEdge::confirmed);

  // Held: inside the wider gate a tracked entry refreshes, at 33.0 and at
  // the exit boundary 35.0 alike.
  for (const double d : {33.0, kGateExitM}) {
    const AdmissionResult r = sim.update(m, d);
    EXPECT_EQ(r.state, TrackState::tracked) << "distance " << d;
    EXPECT_EQ(r.action, AdmissionAction::keep);
    EXPECT_EQ(r.edge, AdmissionEdge::refreshed);
  }

  // Down: one step past the exit gate erases.
  const AdmissionResult dropped = sim.update(m, 35.1);
  EXPECT_EQ(dropped.state, TrackState::not_tracked);
  EXPECT_EQ(dropped.hits, 0);
  EXPECT_EQ(dropped.action, AdmissionAction::erase);
  EXPECT_EQ(dropped.edge, AdmissionEdge::gate_exit);
}

TEST(Admission, PromotionExactlyAtConfirmHitsNotBefore) {
  const AdmissionMachine m = machine();
  Sim sim;
  for (int hit = 1; hit < kConfirmHits; ++hit) {
    const AdmissionResult r = sim.update(m, 20.0);
    EXPECT_EQ(r.state, TrackState::tentative) << "still tentative after hit " << hit;
    EXPECT_EQ(r.hits, hit);
  }
  const AdmissionResult r = sim.update(m, 20.0);
  EXPECT_EQ(r.state, TrackState::tracked);
  EXPECT_EQ(r.edge, AdmissionEdge::confirmed);
}

// --- The Schmitt band while tentative ---------------------------------------

TEST(Admission, TentativeDropsBeyondGateEnterNotGateExit) {
  const AdmissionMachine m = machine();
  Sim sim;
  sim.update(m, 25.0);  // tentative, hits 1

  // 32.0 would hold a tracked entry (≤ GATE_EXIT_M) but drops a tentative
  // one: the narrow gate is the only hold while confirming.
  const AdmissionResult r = sim.update(m, 32.0);
  EXPECT_EQ(r.state, TrackState::not_tracked);
  EXPECT_EQ(r.hits, 0);
  EXPECT_EQ(r.action, AdmissionAction::erase);
  EXPECT_EQ(r.edge, AdmissionEdge::out_of_gate);
}

// --- The counter reset -------------------------------------------------------

TEST(Admission, HitsResetOnDropRequiresFullConfirmAgain) {
  const AdmissionMachine m = machine();
  Sim sim;

  // Two in-gate hits, then an out-of-gate drop.
  sim.update(m, 25.0);
  sim.update(m, 25.0);
  EXPECT_EQ(sim.hits, 2);
  const AdmissionResult dropped = sim.update(m, 31.0);
  EXPECT_EQ(dropped.edge, AdmissionEdge::out_of_gate);
  EXPECT_EQ(sim.hits, 0);

  // Re-entering starts at hits = 1 and needs the full N again: after
  // N − 1 in-gate updates the track is still tentative, never tracked.
  EXPECT_EQ(sim.update(m, 25.0).hits, 1);
  for (int hit = 2; hit < kConfirmHits; ++hit) {
    const AdmissionResult r = sim.update(m, 25.0);
    EXPECT_EQ(r.state, TrackState::tentative) << "prior hits must not survive the drop";
    EXPECT_EQ(r.hits, hit);
  }
  EXPECT_EQ(sim.update(m, 25.0).state, TrackState::tracked);
}

// --- Timeout expiry ----------------------------------------------------------

TEST(Admission, TentativeExpiresStrictlyBeyondTimeout) {
  const AdmissionMachine m = machine();
  Sim sim;
  sim.update(m, 25.0);  // tentative

  // At exactly the timeout nothing fires (the .puml edge is strict >).
  const AdmissionResult held = sim.tick(m, kTimeoutMs);
  EXPECT_EQ(held.state, TrackState::tentative);
  EXPECT_EQ(held.action, AdmissionAction::none);
  EXPECT_EQ(held.edge, AdmissionEdge::none);

  const AdmissionResult expired = sim.tick(m, kTimeoutMs + 1);
  EXPECT_EQ(expired.state, TrackState::not_tracked);
  EXPECT_EQ(expired.hits, 0);
  EXPECT_EQ(expired.action, AdmissionAction::erase);
  EXPECT_EQ(expired.edge, AdmissionEdge::timeout);
}

TEST(Admission, TrackedExpiresStrictlyBeyondTimeout) {
  const AdmissionMachine m = machine();
  Sim sim;
  for (int hit = 0; hit < kConfirmHits; ++hit) sim.update(m, 25.0);
  ASSERT_EQ(sim.state, TrackState::tracked);

  EXPECT_EQ(sim.tick(m, kTimeoutMs).edge, AdmissionEdge::none);

  const AdmissionResult expired = sim.tick(m, kTimeoutMs + 1);
  EXPECT_EQ(expired.state, TrackState::not_tracked);
  EXPECT_EQ(expired.hits, 0);
  EXPECT_EQ(expired.action, AdmissionAction::erase);
  EXPECT_EQ(expired.edge, AdmissionEdge::timeout);
}

TEST(Admission, TimeoutTakesPrecedenceOverInGateUpdate) {
  const AdmissionMachine m = machine();
  Sim sim;
  for (int hit = 0; hit < kConfirmHits; ++hit) sim.update(m, 25.0);
  ASSERT_EQ(sim.state, TrackState::tracked);

  // An in-gate update arriving after over-long silence expires the entry
  // rather than refreshing it (step()'s documented precedence); the next
  // update re-admits from not_tracked.
  const AdmissionResult expired = sim.update(m, 20.0, kTimeoutMs + 500);
  EXPECT_EQ(expired.state, TrackState::not_tracked);
  EXPECT_EQ(expired.action, AdmissionAction::erase);
  EXPECT_EQ(expired.edge, AdmissionEdge::timeout);

  EXPECT_EQ(sim.update(m, 20.0).edge, AdmissionEdge::gate_enter);
}

TEST(Admission, ExpiryTickFiresNothingWhileNotTracked) {
  const AdmissionMachine m = machine();
  Sim sim;
  const AdmissionResult r = sim.tick(m, 10 * kTimeoutMs);
  EXPECT_EQ(r.state, TrackState::not_tracked);
  EXPECT_EQ(r.action, AdmissionAction::none);
  EXPECT_EQ(r.edge, AdmissionEdge::none);
}

// --- Degenerate band: CONFIRM_HITS == 1 -------------------------------------

TEST(Admission, ConfirmHitsOneAdmitsStraightToTracked) {
  const AdmissionMachine m(kGateEnterM, kGateExitM, 1, kTimeoutMs);
  Sim sim;
  const AdmissionResult r = sim.update(m, 25.0);
  EXPECT_EQ(r.state, TrackState::tracked);
  EXPECT_EQ(r.hits, 1);
  EXPECT_EQ(r.action, AdmissionAction::insert);
  EXPECT_EQ(r.edge, AdmissionEdge::confirmed);
}

// --- The reason vocabulary ---------------------------------------------------

TEST(Admission, EdgeNamesMatchTransitionReasonVocabulary) {
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::gate_enter)), "gate_enter");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::counted)), "counted");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::confirmed)), "confirmed");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::refreshed)), "refreshed");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::out_of_gate)), "out_of_gate");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::gate_exit)), "gate_exit");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::timeout)), "timeout");
  EXPECT_EQ(std::string(edgeName(AdmissionEdge::none)), "none");
}

}  // namespace
