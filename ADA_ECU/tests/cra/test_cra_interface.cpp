// CRA interface tests (subtask 14.2.5.1) — proves the frozen R14 seam is
// implementable and freezes its signatures: a minimal fake plugin implements
// ICollisionRiskAssessment, is called through a base pointer, and returns a
// RiskFinding. Deterministic — the store clock is injected.
//
// The header only forward-declares AssessmentDb, so this translation unit
// supplies a trivial definition — legal, and proof the seam lands without the
// accessor subtask. RiskContext holds a reference, so no accessor is touched.

#include "cra/i_collision_risk_assessment.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "contracts/tracked_object.hpp"
#include "store/track_store.hpp"

namespace ada::cra {

// Trivial local definition completing the header's forward declaration.
class AssessmentDb {};

}  // namespace ada::cra

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::cra::AssessmentDb;
using ada::cra::ICollisionRiskAssessment;
using ada::cra::RiskContext;
using ada::cra::RiskFinding;
using ada::store::TrackStore;

constexpr std::int64_t kNowMs = 1789000001000;

// Compile-time freeze of the D4 signatures: a later edit to either pure
// virtual breaks these asserts before any test runs.
static_assert(std::is_same_v<decltype(&ICollisionRiskAssessment::name),
                             std::string (ICollisionRiskAssessment::*)() const>,
              "frozen D4 signature: virtual std::string name() const = 0");
static_assert(std::is_same_v<decltype(&ICollisionRiskAssessment::assess),
                             RiskFinding (ICollisionRiskAssessment::*)(RiskContext&)>,
              "frozen D4 signature: virtual RiskFinding assess(RiskContext&) = 0");
static_assert(std::is_same_v<decltype(RiskContext::store), const TrackStore&>,
              "frozen D4 field: const TrackStore& store");
static_assert(std::is_same_v<decltype(RiskContext::db), AssessmentDb&>,
              "frozen D4 field: AssessmentDb& db");
static_assert(std::is_same_v<decltype(RiskContext::now_ms), std::int64_t>,
              "frozen D4 field: std::int64_t now_ms");
static_assert(std::has_virtual_destructor_v<ICollisionRiskAssessment>,
              "frozen D4: virtual default destructor");

TrackedObject MakeRelayedC() {
  TrackedObject o;
  o.id = "v2x:1001:7";
  o.object_class = "vehicle";
  o.source = Source::v2x_relayed;
  o.position = {45.0, 1.5};
  o.distance = 28.0;
  o.speed = 8.4;
  o.confidence = 0.91;
  o.timestamps = {1789000000073, 1789000000105, 0};
  return o;
}

// The minimal fake: reads the store through the context, returns a finding,
// never emits — transport is the output stage's job (D4).
class FakePlugin final : public ICollisionRiskAssessment {
 public:
  std::string name() const override { return "fake_rule"; }

  RiskFinding assess(RiskContext& ctx) override {
    RiskFinding finding;
    finding.warningType = name();
    finding.riskState = "low";
    finding.trigger = ctx.store.nearest(Source::v2x_relayed);
    finding.rationale = "fake assessment at " + std::to_string(ctx.now_ms);
    return finding;
  }
};

TEST(CraInterface, FakePluginCalledThroughBasePointerReturnsAFinding) {
  TrackStore store([] { return kNowMs; });
  const auto stored = store.upsert(MakeRelayedC());
  AssessmentDb db;
  RiskContext ctx{store, db, kNowMs};

  // Held and called through the base — the seam the registry will use.
  std::unique_ptr<ICollisionRiskAssessment> plugin = std::make_unique<FakePlugin>();
  EXPECT_EQ(plugin->name(), "fake_rule");

  const RiskFinding finding = plugin->assess(ctx);
  EXPECT_EQ(finding.warningType, "fake_rule");
  EXPECT_EQ(finding.riskState, "low");
  ASSERT_TRUE(finding.trigger.has_value());
  EXPECT_TRUE(*finding.trigger == stored);  // the R3 snapshot, as stored
  EXPECT_EQ(finding.rationale, "fake assessment at " + std::to_string(kNowMs));
}

TEST(CraInterface, TriggerIsOptionalWhenTheStoreHoldsNoRelayedTrack) {
  TrackStore store([] { return kNowMs; });
  AssessmentDb db;
  RiskContext ctx{store, db, kNowMs};

  std::unique_ptr<ICollisionRiskAssessment> plugin = std::make_unique<FakePlugin>();
  const RiskFinding finding = plugin->assess(ctx);
  EXPECT_FALSE(finding.trigger.has_value());  // a finding needs no snapshot
  EXPECT_EQ(finding.riskState, "low");
}

}  // namespace
