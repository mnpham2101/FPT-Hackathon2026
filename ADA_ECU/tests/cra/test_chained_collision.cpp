// Tests for the M1 NLOS plugin (subtasks 14.4.1.2, 14.4.1.3).
//
// Band-table half (14.4.1.2): every row of the D5 table with both high
// triggers (range, ttc), both medium triggers, and both low branches (ttc
// null; ttc > RISK_TTC_WARN_S) exercised separately; row order (a state
// satisfying rows 1 and 2 resolves high); ttc null when not closing;
// b_unknown returns low with the exact rationale; the record round-trips
// through the AssessmentDb with the composed values; the upsert cadence
// (creation, committed change, heartbeat).
//
// Dwell half (14.4.1.3): a flap shorter than the dwell commits nothing; a
// level held past the dwell commits once and only once; the full
// low → medium → high → medium → low sequence commits exactly those values in
// order; the clearing transition still carries lastSnapshot after C's track
// was erased.
//
// The store is the ADMISSION-WIRED TrackStore: the plugin filters
// state == tracked, and only admission writes state (D3). Gate values here
// are arbitrary test constants; confirmHits 1 makes an in-gate update
// tracked instantly, and both clocks are the harness's steerable nowMs — so
// everything is deterministic and every *_Ms value lives in one
// CLOCK_MONOTONIC-shaped domain (D10). No sleeps anywhere.
//
// The band-table tests run with RISK_DWELL_MS=0, so a level commits on the
// assess() that observes it; the dwell behaviour has its own tests below with
// RISK_DWELL_MS=300. Thresholds reach the plugin only through config::load
// with an injected env getter — no literal thresholds inside the plugin.

#include "cra/plugins/chained_collision.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "config/config.hpp"
#include "contracts/tracked_object.hpp"
#include "cra/assessment_db.hpp"
#include "cra/registry.hpp"
#include "log/event_log.hpp"
#include "store/track_store.hpp"

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::contracts::Vec2;
using ada::cra::AssessmentDb;
using ada::cra::ChainedCollision;
using ada::cra::registerBuiltinPlugins;
using ada::cra::Registry;
using ada::cra::RiskContext;
using ada::cra::RiskFinding;
using ada::log::EventLog;
using ada::store::TrackStore;
namespace events = ada::log::events;

// Admission gate values: arbitrary test constants (enter 30 / exit 35). The
// gate is on a track's OWN distance while the risk bands are on the composed
// d_AC, so test distances are chosen to satisfy both at once.
constexpr double kGateEnterM = 30.0;
constexpr double kGateExitM = 35.0;
constexpr std::int64_t kTrackTimeoutMs = 1000000;
constexpr const char* kCId = "v2x:1001:7";
constexpr const char* kWarningType = "nlos_obstruction";

TrackedObject MakeTrack(const std::string& id, Source source, double distance, double y) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = source;
  o.position = {distance, y};
  o.distance = distance;
  o.speed = 5.0;
  o.confidence = 0.9;
  o.state = TrackState::not_tracked;  // ignored by the store (D3)
  o.timestamps = {1789000000000, 1789000000050, 0};
  return o;
}

// Loads a Config through the injected env getter — the loader's defaults are
// the HLD §6 values (RISK_NEAR_M 60, RISK_CRITICAL_M 30, RISK_TTC_WARN_S 6,
// RISK_TTC_CRITICAL_S 3, ASSESS_LOG_EVERY_MS 1000); env overrides per test.
ada::config::Config MakeConfig(std::map<std::string, std::string> env) {
  return ada::config::load([env = std::move(env)](const char* key) -> const char* {
    const auto it = env.find(key);
    return it == env.end() ? nullptr : it->second.c_str();
  });
}

// The wired store, the db, the plugin and one steerable clock.
struct Harness {
  std::int64_t nowMs = 100000;
  std::ostringstream sink;
  EventLog log;
  TrackStore store;
  AssessmentDb db;
  ada::config::Config config;
  ChainedCollision plugin;

  explicit Harness(std::map<std::string, std::string> env = {{"RISK_DWELL_MS", "0"}},
                   int confirmHits = 1)
      : log("", &sink,
            EventLog::Clocks{[this] { return nowMs; }, [this] { return nowMs; }}),
        store(kGateEnterM, kGateExitM, confirmHits, kTrackTimeoutMs, log,
              [this] { return nowMs; }, [this] { return nowMs; }),
        db(log),
        config(MakeConfig(std::move(env))),
        plugin(config) {}

  RiskFinding assess() {
    RiskContext context{store, db, nowMs};
    return plugin.assess(context);
  }

  void putOwn(double distance, double y = 0.0, const std::string& id = "own:1") {
    store.apply(MakeTrack(id, Source::own_sensor, distance, y));
  }
  void putRelayed(double distance, double y = 0.0, const std::string& id = kCId) {
    store.apply(MakeTrack(id, Source::v2x_relayed, distance, y));
  }
};

// ---------------------------------------------------------------------------
// Band-table half (14.4.1.2)
// ---------------------------------------------------------------------------

TEST(ChainedCollisionBandTable, NameIsTheFrozenRegistryKey) {
  Harness h;
  EXPECT_EQ(h.plugin.name(), "nlos_obstruction");
}

TEST(ChainedCollisionBandTable, RegistersThroughBuiltinPlugins) {
  // The D4 acceptance shape: one new module plus one registry.add line.
  Harness h;
  Registry registry;
  registerBuiltinPlugins(registry, h.config);
  ASSERT_NE(registry.get("nlos_obstruction"), nullptr);
  EXPECT_EQ(registry.get("nlos_obstruction")->name(), "nlos_obstruction");
}

TEST(ChainedCollisionBandTable, HighByRange) {
  Harness h;
  h.putOwn(10.0, 0.1);
  h.putRelayed(15.0, -0.1);  // d_AC 25 ≤ RISK_CRITICAL_M 30

  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.warningType, kWarningType);
  EXPECT_EQ(finding.riskState, "high");
  EXPECT_EQ(finding.rationale, "range");
  ASSERT_TRUE(finding.trigger.has_value());
  EXPECT_EQ(finding.trigger->id, kCId);
  EXPECT_EQ(finding.trigger->source, Source::v2x_relayed);
}

TEST(ChainedCollisionBandTable, HighByTtc) {
  Harness h;
  h.putOwn(20.0);
  h.putRelayed(25.0);
  EXPECT_EQ(h.assess().riskState, "medium");  // d_AC 45: row 2 by range

  h.nowMs += 1000;
  h.putRelayed(11.0);  // d_AC 31 > 30, rate 14 m/s → ttc ≈ 2.21 ≤ 3
  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.riskState, "high");
  EXPECT_EQ(finding.rationale, "ttc");
}

TEST(ChainedCollisionBandTable, MediumByRange) {
  Harness h;
  h.putOwn(20.0, 0.5);
  h.putRelayed(25.0, -0.3);  // d_AC 45 ≤ RISK_NEAR_M 60, ttc null (first sample)

  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.riskState, "medium");
  EXPECT_EQ(finding.rationale, "range");
}

TEST(ChainedCollisionBandTable, MediumByTtcBeyondNearRange) {
  // RISK_TTC_WARN_S's whole effect: a track closing fast enough is medium
  // before its range reaches RISK_NEAR_M.
  Harness h;
  h.putOwn(28.0);
  h.putOwn(34.0);  // B walked out to 34 — a tracked entry holds ≤ GATE_EXIT_M
  h.putRelayed(28.0);
  h.putRelayed(34.0);                       // d_AC 68 > 60
  EXPECT_EQ(h.assess().riskState, "low");   // ttc null on the first sample

  h.nowMs += 200;
  h.putRelayed(31.0);  // d_AC 65 > 60, rate 15 m/s → ttc ≈ 4.33 in (3, 6]
  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.riskState, "medium");
  EXPECT_EQ(finding.rationale, "ttc");
}

TEST(ChainedCollisionBandTable, RowOrderHighWinsWhenRowsOneAndTwoBothMatch) {
  // d_AC 25 satisfies row 1 (≤ 30) and row 2 (≤ 60): the first matching row
  // wins — the state resolves high, never medium.
  Harness h;
  h.putOwn(10.0);
  h.putRelayed(15.0);
  EXPECT_EQ(h.assess().riskState, "high");
}

TEST(ChainedCollisionBandTable, LowWhenTtcNullAndBeyondNear) {
  Harness h;
  h.putOwn(28.0);
  h.putOwn(34.0);
  h.putRelayed(28.0);
  h.putRelayed(33.0);  // d_AC 67 > 60

  EXPECT_EQ(h.assess().riskState, "low");  // ttc null: no prior sample

  h.nowMs += 1000;
  h.putRelayed(34.0);  // receding: rate < 0 → ttc null when not closing
  const RiskFinding finding = h.assess();
  EXPECT_EQ(finding.riskState, "low");
}

TEST(ChainedCollisionBandTable, LowWhenTtcAboveWarn) {
  Harness h;
  h.putOwn(28.0);
  h.putOwn(34.0);
  h.putRelayed(28.0);
  h.putRelayed(34.0);  // d_AC 68 > 60
  EXPECT_EQ(h.assess().riskState, "low");

  h.nowMs += 1000;
  h.putRelayed(33.0);  // d_AC 67, rate 1 m/s → ttc 67 > RISK_TTC_WARN_S 6
  EXPECT_EQ(h.assess().riskState, "low");
}

TEST(ChainedCollisionBandTable, BUnknownReturnsLowWithTheExactRationale) {
  // No own-sensor B and no remembered lastKnownB: d_AC does not exist. The
  // exact rationale string is load-bearing — the controller matches it to
  // emit assess_skipped_b_unknown.
  Harness h;
  h.putRelayed(25.0);

  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.riskState, "low");
  EXPECT_EQ(finding.rationale, "b_unknown");
  ASSERT_TRUE(finding.trigger.has_value());
  EXPECT_EQ(finding.trigger->id, kCId);
  // No record is written without a B, so no medium/high can ever have been
  // entered without one.
  EXPECT_FALSE(h.db.get(kCId, kWarningType).has_value());
  EXPECT_EQ(h.log.count(events::kAssessment), 0u);
}

TEST(ChainedCollisionBandTable, NoTrackedCReturnsLowWithoutDbWrites) {
  Harness h;
  h.putOwn(20.0);

  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.riskState, "low");
  EXPECT_EQ(finding.rationale, "no_tracked_c");
  EXPECT_FALSE(finding.trigger.has_value());
  EXPECT_EQ(h.log.count(events::kAssessment), 0u);
}

TEST(ChainedCollisionBandTable, TentativeCDoesNotCount) {
  // Only `tracked` counts: with confirmHits 3, one in-gate update leaves C
  // tentative, and the plugin sees no C at all.
  Harness h({{"RISK_DWELL_MS", "0"}}, /*confirmHits=*/3);
  h.putOwn(20.0);
  h.putRelayed(25.0);
  ASSERT_EQ(h.store.get(kCId)->state, TrackState::tentative);

  const RiskFinding finding = h.assess();

  EXPECT_EQ(finding.riskState, "low");
  EXPECT_EQ(finding.rationale, "no_tracked_c");
}

TEST(ChainedCollisionBandTable, NearestTrackedRelayedIsC) {
  Harness h;
  h.putOwn(20.0);
  h.putRelayed(25.0, 0.0, "v2x:1001:7");
  h.putRelayed(20.0, 0.0, "v2x:1001:9");  // nearer: this one is C

  const RiskFinding finding = h.assess();  // d_AC 40 → medium

  EXPECT_EQ(finding.riskState, "medium");
  ASSERT_TRUE(finding.trigger.has_value());
  EXPECT_EQ(finding.trigger->id, "v2x:1001:9");
  EXPECT_TRUE(h.db.get("v2x:1001:9", kWarningType).has_value());
  EXPECT_FALSE(h.db.get("v2x:1001:7", kWarningType).has_value());
}

TEST(ChainedCollisionBandTable, RecordRoundTripsWithTheComposedValues) {
  Harness h;
  h.putOwn(20.0, 0.5);
  h.putRelayed(25.0, -0.3);
  h.assess();

  const auto record = h.db.get(kCId, kWarningType);
  ASSERT_TRUE(record.has_value());
  EXPECT_EQ(record->trackId, kCId);
  EXPECT_EQ(record->warningType, kWarningType);
  EXPECT_EQ(record->riskState, "medium");
  EXPECT_DOUBLE_EQ(record->distanceM, 45.0);          // composed d_AC
  EXPECT_DOUBLE_EQ(record->previousDistanceM, 45.0);  // first sample: same
  EXPECT_DOUBLE_EQ(record->closingRateMps, 0.0);
  EXPECT_FALSE(record->ttcS.has_value());
  ASSERT_TRUE(record->lastKnownB.has_value());
  EXPECT_EQ(*record->lastKnownB, (Vec2{20.0, 0.5}));
  EXPECT_EQ(record->lastSnapshot.id, kCId);
  EXPECT_EQ(record->lastSnapshot.state, TrackState::tracked);
  EXPECT_EQ(record->firstSeenMs, h.nowMs);
  EXPECT_EQ(record->riskStateEnteredMs, h.nowMs);
  EXPECT_EQ(record->emittedCount, 0);
  EXPECT_EQ(record->rationale, "range");
  EXPECT_EQ(h.db.all().size(), 1u);
}

TEST(ChainedCollisionBandTable, UpsertCadenceIsCreationCommitAndHeartbeat) {
  // ASSESS_LOG_EVERY_MS default 1000. upsert() IS the [EVT] assessment line,
  // so the assessment count is the upsert count.
  Harness h;
  h.putOwn(20.0);
  h.putRelayed(25.0);

  h.assess();  // (a) creation at low + (b) the committed change to medium
  EXPECT_EQ(h.log.count(events::kAssessment), 2u);

  h.nowMs += 500;
  h.assess();  // steady state between upserts: nothing
  EXPECT_EQ(h.log.count(events::kAssessment), 2u);

  h.nowMs += 500;
  h.assess();  // (c) 1000 ms since the last upsert: heartbeat
  EXPECT_EQ(h.log.count(events::kAssessment), 3u);
}
