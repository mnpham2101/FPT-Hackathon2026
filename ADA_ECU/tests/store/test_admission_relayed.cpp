// Store ↔ admission wiring tests, relayed source (13.2.4.3) — the unit-level
// closure of the "admitted only within gate_enter, dropped only beyond
// gate_exit or after the timeout, no flicker" box. Every stimulus is a
// relayed-shaped update through TrackStore::apply(); the emitted [EVT] lines
// are captured through an injected string sink (the test_event_log pattern),
// and both clocks are injected — deterministic, no sleeps.
//
// Thresholds mirror the HLD §6 defaults (GATE_ENTER_M 30, GATE_EXIT_M 35,
// TRACK_TIMEOUT_MS 1000) as plain test constants, passed through the wired
// constructor exactly as main will pass the loaded Config values.

#include "store/track_store.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "log/event_log.hpp"

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::log::EventLog;
using ada::store::TrackStore;
namespace events = ada::log::events;

constexpr double kGateEnterM = 30.0;
constexpr double kGateExitM = 35.0;
constexpr std::int64_t kTimeoutMs = 1000;
constexpr const char* kRelayedId = "v2x:1001:7";

TrackedObject MakeRelayed(double distance, const std::string& id = kRelayedId) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = Source::v2x_relayed;
  o.position = {distance, 0.0};
  o.distance = distance;
  o.speed = 5.0;
  o.confidence = 0.9;
  o.state = TrackState::not_tracked;  // ignored by the store (D3)
  o.timestamps = {1789000000000, 1789000000050, 0};
  return o;
}

// A wired store over steerable clocks and a captured [EVT] sink.
struct Harness {
  std::int64_t monoMs = 100000;
  std::int64_t epochMs = 1789000001000;
  std::ostringstream sink;
  EventLog log;
  TrackStore store;

  explicit Harness(int confirmHits)
      : log("", &sink,
            EventLog::Clocks{[this] { return monoMs; }, [this] { return epochMs; }}),
        store(kGateEnterM, kGateExitM, confirmHits, kTimeoutMs, log,
              [this] { return epochMs; }, [this] { return monoMs; }) {}

  // Every captured payload of one event name, in emission order.
  std::vector<nlohmann::json> payloads(const std::string& event) const {
    std::vector<nlohmann::json> out;
    std::istringstream stream(sink.str());
    std::string line;
    const std::string prefix = "[EVT] ";
    while (std::getline(stream, line)) {
      EXPECT_EQ(line.rfind(prefix, 0), 0u) << "line lacks the [EVT] prefix: " << line;
      const nlohmann::json j = nlohmann::json::parse(line.substr(prefix.size()));
      if (j.at("event") == event) {
        out.push_back(j.at("payload"));
      }
    }
    return out;
  }

  std::vector<nlohmann::json> transitions() const { return payloads(events::kTrackTransition); }
};

// --- The admit gate, boundary-exact (rule: distance ≤ GATE_ENTER_M) ---------

TEST(AdmissionRelayed, AdmitsAtAndInsideGateEnter) {
  for (const double d : {29.9, 30.0}) {
    Harness h(/*confirmHits=*/3);
    h.store.apply(MakeRelayed(d));

    const auto entry = h.store.get(kRelayedId);
    ASSERT_TRUE(entry.has_value()) << "distance " << d;
    EXPECT_EQ(entry->state, TrackState::tentative);

    const auto t = h.transitions();
    ASSERT_EQ(t.size(), 1u) << "distance " << d;
    EXPECT_EQ(t[0].at("id"), kRelayedId);
    EXPECT_EQ(t[0].at("source"), "v2x_relayed");
    EXPECT_EQ(t[0].at("from"), "not_tracked");
    EXPECT_EQ(t[0].at("to"), "tentative");
    EXPECT_EQ(t[0].at("distance"), d);
    EXPECT_EQ(t[0].at("reason"), "gate_enter");
  }
}

TEST(AdmissionRelayed, DoesNotAdmitBeyondGateEnter) {
  Harness h(/*confirmHits=*/3);
  h.store.apply(MakeRelayed(30.1));

  EXPECT_TRUE(h.store.empty());
  EXPECT_EQ(h.log.count(events::kTrackTransition), 0u);
  EXPECT_EQ(h.log.count(events::kTrackExpire), 0u);
}

// --- The exit gate once tracked, boundary-exact (rule: ≤ GATE_EXIT_M holds) --

TEST(AdmissionRelayed, HoldsAtAndInsideGateExitOnceTracked) {
  for (const double d : {34.9, 35.0}) {
    Harness h(/*confirmHits=*/1);
    h.store.apply(MakeRelayed(20.0));  // confirmHits 1: straight to tracked
    ASSERT_EQ(h.store.get(kRelayedId)->state, TrackState::tracked);
    ASSERT_EQ(h.transitions().size(), 1u);

    h.monoMs += 100;
    h.store.apply(MakeRelayed(d));

    const auto entry = h.store.get(kRelayedId);
    ASSERT_TRUE(entry.has_value()) << "distance " << d;
    EXPECT_EQ(entry->state, TrackState::tracked);
    EXPECT_EQ(entry->distance, d);
    // refreshed is a self-loop: no further track_transition.
    EXPECT_EQ(h.transitions().size(), 1u) << "distance " << d;
  }
}

TEST(AdmissionRelayed, DropsBeyondGateExitOnceTracked) {
  Harness h(/*confirmHits=*/1);
  h.store.apply(MakeRelayed(20.0));
  ASSERT_EQ(h.store.get(kRelayedId)->state, TrackState::tracked);

  h.monoMs += 100;
  h.store.apply(MakeRelayed(35.1));

  EXPECT_FALSE(h.store.get(kRelayedId).has_value());  // erased, not flagged
  const auto t = h.transitions();
  ASSERT_EQ(t.size(), 2u);
  EXPECT_EQ(t[1].at("from"), "tracked");
  EXPECT_EQ(t[1].at("to"), "not_tracked");
  EXPECT_EQ(t[1].at("distance"), 35.1);
  EXPECT_EQ(t[1].at("reason"), "gate_exit");
  // A gate drop is not an expiry.
  EXPECT_EQ(h.log.count(events::kTrackExpire), 0u);
}

// --- No flicker across the hysteresis band -----------------------------------

TEST(AdmissionRelayed, OscillatingInsideTheBandYieldsOneAdmitAndZeroDrops) {
  Harness h(/*confirmHits=*/1);
  // 30.0 admits (≤ GATE_ENTER_M); every later value sits in the 30–35 band,
  // which holds a tracked entry (≤ GATE_EXIT_M) — the Schmitt band.
  for (const double d : {30.0, 34.0, 30.1, 33.9, 34.9, 30.5, 34.0}) {
    h.store.apply(MakeRelayed(d));
    h.monoMs += 100;  // well inside the timeout
  }

  const auto t = h.transitions();
  ASSERT_EQ(t.size(), 1u);  // exactly one admit transition, nothing else
  EXPECT_EQ(t[0].at("to"), "tracked");
  EXPECT_EQ(t[0].at("reason"), "confirmed");
  EXPECT_EQ(h.log.count(events::kTrackExpire), 0u);
  ASSERT_TRUE(h.store.get(kRelayedId).has_value());
  EXPECT_EQ(h.store.get(kRelayedId)->state, TrackState::tracked);
}

// --- Expiry after TRACK_TIMEOUT_MS of silence --------------------------------

TEST(AdmissionRelayed, ExpiresAfterTimeoutOfSilence) {
  Harness h(/*confirmHits=*/1);
  h.store.apply(MakeRelayed(25.0));
  const std::int64_t admittedAt = h.monoMs;

  // At exactly the timeout nothing fires (the edge is strict >).
  EXPECT_EQ(h.store.expire(admittedAt + kTimeoutMs), 0u);
  EXPECT_TRUE(h.store.get(kRelayedId).has_value());

  ASSERT_EQ(h.store.expire(admittedAt + kTimeoutMs + 1), 1u);
  EXPECT_TRUE(h.store.empty());

  const auto t = h.transitions();
  ASSERT_EQ(t.size(), 2u);
  EXPECT_EQ(t[1].at("id"), kRelayedId);
  EXPECT_EQ(t[1].at("source"), "v2x_relayed");
  EXPECT_EQ(t[1].at("from"), "tracked");
  EXPECT_EQ(t[1].at("to"), "not_tracked");
  EXPECT_EQ(t[1].at("distance"), 25.0);  // the last accepted observation
  EXPECT_EQ(t[1].at("reason"), "timeout");

  // Expiry additionally emits track_expire.
  const auto expired = h.payloads(events::kTrackExpire);
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_EQ(expired[0].at("id"), kRelayedId);
  EXPECT_EQ(expired[0].at("source"), "v2x_relayed");
}

TEST(AdmissionRelayed, TentativeExpiresOnSilenceToo) {
  Harness h(/*confirmHits=*/3);
  h.store.apply(MakeRelayed(29.0));  // tentative, hits 1
  ASSERT_EQ(h.store.get(kRelayedId)->state, TrackState::tentative);

  ASSERT_EQ(h.store.expire(h.monoMs + kTimeoutMs + 1), 1u);
  EXPECT_TRUE(h.store.empty());
  const auto t = h.transitions();
  ASSERT_EQ(t.size(), 2u);
  EXPECT_EQ(t[1].at("from"), "tentative");
  EXPECT_EQ(t[1].at("reason"), "timeout");
  EXPECT_EQ(h.log.count(events::kTrackExpire), 1u);
}

TEST(AdmissionRelayed, StaleUpdateExpiresThenReAdmits) {
  Harness h(/*confirmHits=*/1);
  h.store.apply(MakeRelayed(25.0));

  // An in-gate update after over-long silence expires the entry (the
  // machine's timeout-first precedence); the very next update re-admits.
  h.monoMs += kTimeoutMs + 500;
  h.store.apply(MakeRelayed(24.0));
  EXPECT_TRUE(h.store.empty());
  EXPECT_EQ(h.log.count(events::kTrackExpire), 1u);

  h.store.apply(MakeRelayed(24.0));
  ASSERT_TRUE(h.store.get(kRelayedId).has_value());

  std::vector<std::string> reasons;
  for (const auto& p : h.transitions()) {
    reasons.push_back(p.at("reason").get<std::string>());
  }
  EXPECT_EQ(reasons, (std::vector<std::string>{"confirmed", "timeout", "confirmed"}));
  // The timeout transition carries the last ACCEPTED distance, not the
  // rejected stale observation's.
  EXPECT_EQ(h.transitions()[1].at("distance"), 25.0);
}

// --- The full lifecycle matches the diagram edge for edge --------------------

TEST(AdmissionRelayed, TransitionSequenceMatchesTheDiagram) {
  Harness h(/*confirmHits=*/3);

  h.store.apply(MakeRelayed(29.0));  // NT → TE: gate_enter
  h.monoMs += 100;
  h.store.apply(MakeRelayed(28.0));  // TE → TE: counted — no event
  h.monoMs += 100;
  h.store.apply(MakeRelayed(27.0));  // TE → TR: confirmed
  h.monoMs += 100;
  h.store.apply(MakeRelayed(33.0));  // TR → TR: refreshed — no event
  h.monoMs += 100;
  h.store.apply(MakeRelayed(36.0));  // TR → NT: gate_exit

  const auto t = h.transitions();
  ASSERT_EQ(t.size(), 3u);  // the two self-loops emitted nothing

  EXPECT_EQ(t[0].at("from"), "not_tracked");
  EXPECT_EQ(t[0].at("to"), "tentative");
  EXPECT_EQ(t[0].at("distance"), 29.0);
  EXPECT_EQ(t[0].at("reason"), "gate_enter");

  EXPECT_EQ(t[1].at("from"), "tentative");
  EXPECT_EQ(t[1].at("to"), "tracked");
  EXPECT_EQ(t[1].at("distance"), 27.0);
  EXPECT_EQ(t[1].at("reason"), "confirmed");

  EXPECT_EQ(t[2].at("from"), "tracked");
  EXPECT_EQ(t[2].at("to"), "not_tracked");
  EXPECT_EQ(t[2].at("distance"), 36.0);
  EXPECT_EQ(t[2].at("reason"), "gate_exit");

  // Every transition names this relayed track, and carries exactly the six
  // frozen payload fields: id, source, from, to, distance, reason.
  for (const auto& p : t) {
    EXPECT_EQ(p.at("id"), kRelayedId);
    EXPECT_EQ(p.at("source"), "v2x_relayed");
    EXPECT_EQ(p.size(), 6u);
  }

  EXPECT_TRUE(h.store.empty());
  EXPECT_EQ(h.log.count(events::kTrackExpire), 0u);
}

}  // namespace
