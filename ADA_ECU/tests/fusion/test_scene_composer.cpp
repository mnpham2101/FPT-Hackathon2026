// Tests for the ego-frame scene composer (subtask 15.4.1.1): composed values
// against hand-computed numbers; both null-C cases (before C is first
// admitted, and after its track is erased — the frozen R4 null case); the
// not-composable case (no own-sensor B, no remembered fallback); and
// nearest-B selection among several own-sensor tracks.
//
// The store is the data-only TrackStore: upsert() inserts entries as
// not_tracked and nearest() does not filter by state, so plain upserts are
// all these tests need. The `tracked`-only rule for C is the caller's
// (14.4.1.2), enforced before compose() is reached.

#include "fusion/scene_composer.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "contracts/tracked_object.hpp"
#include "store/track_store.hpp"

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::contracts::Vec2;
using ada::fusion::compose;
using ada::fusion::SceneGeometry;
using ada::store::TrackStore;

TrackedObject MakeTrack(const std::string& id, Source source, double distance, double y) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = source;
  o.position = {distance, y};
  o.distance = distance;
  o.speed = 5.0;
  o.confidence = 0.9;
  o.state = TrackState::not_tracked;  // the store preserves its own state (D3)
  o.timestamps = {1789000000000, 1789000000050, 0};
  return o;
}

// --- Composed values against hand-computed numbers ---------------------------

TEST(SceneComposer, ComposedValuesMatchHandComputedSets) {
  struct Case {
    double dAb, yB, dBc, yBc;
  };
  // Hand-computed: vehicleC = (dAb + dBc, yB + yBc).
  //   (20.0, 0.5, 25.0, -0.3) → C (45.0, 0.2)
  //   (18.1, 0.2, 28.4,  0.4) → C (46.5, 0.6)
  //   (10.0, -1.0, 15.5, 1.0) → C (25.5, 0.0)
  const Case cases[] = {
      {20.0, 0.5, 25.0, -0.3}, {18.1, 0.2, 28.4, 0.4}, {10.0, -1.0, 15.5, 1.0}};

  for (const Case& c : cases) {
    TrackStore store;
    store.upsert(MakeTrack("own:1", Source::own_sensor, c.dAb, c.yB));
    const TrackedObject trackedC =
        MakeTrack("v2x:1001:7", Source::v2x_relayed, c.dBc, c.yBc);

    const auto geometry = compose(store, trackedC, std::nullopt);

    ASSERT_TRUE(geometry.has_value()) << "d_AB " << c.dAb;
    EXPECT_DOUBLE_EQ(geometry->ego.x, 0.0);
    EXPECT_DOUBLE_EQ(geometry->ego.y, 0.0);
    EXPECT_DOUBLE_EQ(geometry->vehicleB.x, c.dAb);
    EXPECT_DOUBLE_EQ(geometry->vehicleB.y, c.yB);
    ASSERT_TRUE(geometry->vehicleC.has_value());
    EXPECT_DOUBLE_EQ(geometry->vehicleC->x, c.dAb + c.dBc);
    EXPECT_DOUBLE_EQ(geometry->vehicleC->y, c.yB + c.yBc);
  }
}

// --- Null C is legitimate both before admission and after erasure ------------

TEST(SceneComposer, NullCBeforeCIsFirstAdmitted) {
  TrackStore store;
  store.upsert(MakeTrack("own:1", Source::own_sensor, 20.0, 0.5));

  const auto geometry = compose(store, std::nullopt, std::nullopt);

  ASSERT_TRUE(geometry.has_value());
  EXPECT_DOUBLE_EQ(geometry->vehicleB.x, 20.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleB.y, 0.5);
  EXPECT_FALSE(geometry->vehicleC.has_value());
}

TEST(SceneComposer, NullCAfterErasureComposesFromFallbackB) {
  // After C's track (and B's) is erased, the remembered lastKnownB still
  // yields a geometry — vehicleB numeric, vehicleC null (the frozen schema's
  // "null until C is first tracked" case, which erasure re-enters).
  TrackStore store;  // empty: everything erased

  const auto geometry = compose(store, std::nullopt, Vec2{20.0, 0.5});

  ASSERT_TRUE(geometry.has_value());
  EXPECT_DOUBLE_EQ(geometry->ego.x, 0.0);
  EXPECT_DOUBLE_EQ(geometry->ego.y, 0.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleB.x, 20.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleB.y, 0.5);
  EXPECT_FALSE(geometry->vehicleC.has_value());
}

TEST(SceneComposer, FallbackBStillComposesC) {
  // No own-sensor B in the store, but the record remembered one: the
  // composition works on the remembered values — the clearing-event path.
  TrackStore store;
  const TrackedObject trackedC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 25.0, -0.3);

  const auto geometry = compose(store, trackedC, Vec2{20.0, 0.5});

  ASSERT_TRUE(geometry.has_value());
  EXPECT_DOUBLE_EQ(geometry->vehicleB.x, 20.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleB.y, 0.5);
  ASSERT_TRUE(geometry->vehicleC.has_value());
  EXPECT_DOUBLE_EQ(geometry->vehicleC->x, 45.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleC->y, 0.2);
}

// --- Not composable: no B, no fallback ---------------------------------------

TEST(SceneComposer, NotComposableWithoutBOrFallback) {
  TrackStore store;
  const TrackedObject trackedC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 25.0, 0.0);

  EXPECT_FALSE(compose(store, trackedC, std::nullopt).has_value());
  EXPECT_FALSE(compose(store, std::nullopt, std::nullopt).has_value());

  // A relayed entry can never stand in for B: still not composable.
  store.upsert(MakeTrack("v2x:1001:8", Source::v2x_relayed, 5.0, 0.0));
  EXPECT_FALSE(compose(store, trackedC, std::nullopt).has_value());
}

// --- Nearest-B selection ------------------------------------------------------

TEST(SceneComposer, NearestOwnSensorTrackIsB) {
  TrackStore store;
  store.upsert(MakeTrack("own:1", Source::own_sensor, 30.0, -0.4));
  store.upsert(MakeTrack("own:2", Source::own_sensor, 12.0, 0.7));
  store.upsert(MakeTrack("own:3", Source::own_sensor, 22.0, 0.1));
  // A closer relayed entry must not win B's role.
  store.upsert(MakeTrack("v2x:1001:8", Source::v2x_relayed, 5.0, 0.0));
  const TrackedObject trackedC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 10.0, 0.0);

  const auto geometry = compose(store, trackedC, std::nullopt);

  ASSERT_TRUE(geometry.has_value());
  EXPECT_DOUBLE_EQ(geometry->vehicleB.x, 12.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleB.y, 0.7);
  ASSERT_TRUE(geometry->vehicleC.has_value());
  EXPECT_DOUBLE_EQ(geometry->vehicleC->x, 22.0);
  EXPECT_DOUBLE_EQ(geometry->vehicleC->y, 0.7);
}

}  // namespace
