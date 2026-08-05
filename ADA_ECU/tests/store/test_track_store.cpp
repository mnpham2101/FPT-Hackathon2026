// Track store tests (subtask 3.2.4.1) — the unit-level closure of the R3
// acceptance box: all nine R3 fields exposed, one identical upsert for both
// sources, store-owned state, injected-clock lastUpdated. Deterministic — the
// clock is injected, never read from the system.

#include "store/track_store.hpp"

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::store::TrackStore;

constexpr std::int64_t kT1 = 1789000001000;
constexpr std::int64_t kT2 = 1789000002500;

// An object carrying every R3 field, deliberately including a non-default
// state and lastUpdated — both of which the store must overwrite.
TrackedObject MakeObject(const std::string& id, Source source, double distance) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = source;
  o.position = {12.5, -3.25};
  o.distance = distance;
  o.speed = 8.4;
  o.confidence = 0.91;
  o.state = TrackState::tracked;                          // ignored by the store (D3)
  o.timestamps = {1789000000073, 1789000000105, 999};     // lastUpdated 999 is discarded
  return o;
}

TEST(TrackStore, RoundTripsAllNineR3Fields) {
  std::int64_t now = kT1;
  TrackStore store([&now] { return now; });

  const auto in = MakeObject("v2x:1001:7", Source::v2x_relayed, 42.5);
  store.upsert(in);

  const auto out = store.get("v2x:1001:7");
  ASSERT_TRUE(out.has_value());

  // 1 id · 2 class · 3 source · 4 position · 5 distance · 6 speed ·
  // 7 confidence — taken from the incoming object.
  EXPECT_EQ(out->id, "v2x:1001:7");
  EXPECT_EQ(out->object_class, "vehicle");
  EXPECT_EQ(out->source, Source::v2x_relayed);
  EXPECT_EQ(out->position.x, 12.5);
  EXPECT_EQ(out->position.y, -3.25);
  EXPECT_EQ(out->distance, 42.5);
  EXPECT_EQ(out->speed, 8.4);
  EXPECT_EQ(out->confidence, 0.91);
  // 8 state — store-owned: a first insert enters as not_tracked, the incoming
  // tracked is ignored (D3).
  EXPECT_EQ(out->state, TrackState::not_tracked);
  // 9 timestamps — measured and received preserved, lastUpdated stamped from
  // the injected clock, the incoming 999 discarded.
  EXPECT_EQ(out->timestamps.measured, static_cast<std::int64_t>(1789000000073));
  EXPECT_EQ(out->timestamps.received, static_cast<std::int64_t>(1789000000105));
  EXPECT_EQ(out->timestamps.lastUpdated, kT1);
}

TEST(TrackStore, BothSourcesEnterTheSameUpsertIndistinguishableExceptSource) {
  std::int64_t now = kT1;
  TrackStore store([&now] { return now; });

  // Detector-shaped and relayed-shaped, identical except id namespace and
  // source, through the one upsert (the R3 acceptance box).
  store.upsert(MakeObject("own:1", Source::own_sensor, 42.5));
  store.upsert(MakeObject("v2x:1001:7", Source::v2x_relayed, 42.5));

  const auto own = store.get("own:1");
  const auto relayed = store.get("v2x:1001:7");
  ASSERT_TRUE(own.has_value());
  ASSERT_TRUE(relayed.has_value());
  EXPECT_EQ(own->source, Source::own_sensor);
  EXPECT_EQ(relayed->source, Source::v2x_relayed);

  // Erase what legitimately differs (id, source); everything else the store
  // holds must be identical — same state, same stamps, same data fields.
  auto a = *own;
  auto b = *relayed;
  a.id = b.id = "";
  a.source = b.source = Source::own_sensor;
  EXPECT_TRUE(a == b);
}

TEST(TrackStore, AllAndAllBySourceFilterAndOrderById) {
  TrackStore store([] { return kT1; });
  store.upsert(MakeObject("own:2", Source::own_sensor, 20.0));
  store.upsert(MakeObject("own:1", Source::own_sensor, 30.0));
  store.upsert(MakeObject("v2x:1001:7", Source::v2x_relayed, 25.0));

  const auto everything = store.all();
  ASSERT_EQ(everything.size(), 3u);
  EXPECT_EQ(everything[0].id, "own:1");  // std::map id order — deterministic
  EXPECT_EQ(everything[1].id, "own:2");
  EXPECT_EQ(everything[2].id, "v2x:1001:7");

  const auto ownOnly = store.allBySource(Source::own_sensor);
  ASSERT_EQ(ownOnly.size(), 2u);
  EXPECT_EQ(ownOnly[0].id, "own:1");
  EXPECT_EQ(ownOnly[1].id, "own:2");

  const auto relayedOnly = store.allBySource(Source::v2x_relayed);
  ASSERT_EQ(relayedOnly.size(), 1u);
  EXPECT_EQ(relayedOnly[0].id, "v2x:1001:7");
}

TEST(TrackStore, NearestPicksSmallestDistancePerSource) {
  TrackStore store([] { return kT1; });
  store.upsert(MakeObject("own:1", Source::own_sensor, 30.0));
  store.upsert(MakeObject("own:2", Source::own_sensor, 12.0));
  store.upsert(MakeObject("v2x:1001:7", Source::v2x_relayed, 55.0));
  store.upsert(MakeObject("v2x:1001:9", Source::v2x_relayed, 28.0));

  const auto nearestOwn = store.nearest(Source::own_sensor);
  ASSERT_TRUE(nearestOwn.has_value());
  EXPECT_EQ(nearestOwn->id, "own:2");
  EXPECT_EQ(nearestOwn->distance, 12.0);

  const auto nearestRelayed = store.nearest(Source::v2x_relayed);
  ASSERT_TRUE(nearestRelayed.has_value());
  EXPECT_EQ(nearestRelayed->id, "v2x:1001:9");
  EXPECT_EQ(nearestRelayed->distance, 28.0);
}

TEST(TrackStore, NearestIsEmptyForAnUnrepresentedSourceAndBreaksTiesById) {
  TrackStore store([] { return kT1; });
  EXPECT_FALSE(store.nearest(Source::own_sensor).has_value());

  store.upsert(MakeObject("own:2", Source::own_sensor, 15.0));
  store.upsert(MakeObject("own:1", Source::own_sensor, 15.0));
  EXPECT_FALSE(store.nearest(Source::v2x_relayed).has_value());

  const auto nearest = store.nearest(Source::own_sensor);
  ASSERT_TRUE(nearest.has_value());
  EXPECT_EQ(nearest->id, "own:1");  // exact tie → smallest id
}

TEST(TrackStore, EraseRemovesTheEntry) {
  TrackStore store([] { return kT1; });
  store.upsert(MakeObject("own:1", Source::own_sensor, 20.0));
  ASSERT_EQ(store.size(), 1u);

  EXPECT_TRUE(store.erase("own:1"));
  EXPECT_FALSE(store.get("own:1").has_value());
  EXPECT_TRUE(store.empty());
  EXPECT_FALSE(store.erase("own:1"));  // already absent
}

TEST(TrackStore, UpsertPreservesStoredStateAndRestampsLastUpdated) {
  std::int64_t now = kT1;
  TrackStore store([&now] { return now; });

  store.upsert(MakeObject("own:1", Source::own_sensor, 20.0));
  ASSERT_EQ(store.get("own:1")->state, TrackState::not_tracked);
  ASSERT_EQ(store.get("own:1")->timestamps.lastUpdated, kT1);

  // A refresh carrying a different state and new data: the stored state is
  // preserved — never taken from the incoming object — while every data
  // field updates and lastUpdated is restamped with the injected clock.
  now = kT2;
  auto refresh = MakeObject("own:1", Source::own_sensor, 18.5);
  refresh.state = TrackState::tentative;
  const auto& stored = store.upsert(refresh);

  EXPECT_EQ(stored.state, TrackState::not_tracked);
  EXPECT_EQ(stored.distance, 18.5);
  EXPECT_EQ(stored.timestamps.lastUpdated, kT2);
  EXPECT_EQ(store.size(), 1u);  // refreshed in place, not duplicated
}

}  // namespace
