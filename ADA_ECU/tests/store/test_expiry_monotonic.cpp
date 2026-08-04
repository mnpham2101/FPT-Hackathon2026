// Expiry clock-domain tests (13.2.4.3) — the failure pair D10 exists to
// prevent: a step on the shared wall clock (CLOCK_REALTIME, the
// timestamps.lastUpdated domain) must never expire a track, and monotonic
// silence alone must. Both clocks are injected and steered independently —
// deterministic, no sleeps.

#include "store/track_store.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <sstream>
#include <string>

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
constexpr int kConfirmHits = 1;
constexpr std::int64_t kTimeoutMs = 1000;

constexpr std::int64_t kMono0 = 100000;          // monotonic origin, ms
constexpr std::int64_t kEpoch0 = 1789000001000;  // realtime origin, ms
constexpr std::int64_t kClockStepMs = 3600000;   // a one-hour NTP-style step

TrackedObject MakeObject(const std::string& id, Source source, double distance) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = source;
  o.position = {distance, 0.0};
  o.distance = distance;
  o.speed = 5.0;
  o.confidence = 0.9;
  o.state = TrackState::not_tracked;
  o.timestamps = {1789000000000, 1789000000050, 0};
  return o;
}

struct Harness {
  std::int64_t monoMs = kMono0;
  std::int64_t epochMs = kEpoch0;
  std::ostringstream sink;
  EventLog log;
  TrackStore store;

  Harness()
      : log("", &sink,
            EventLog::Clocks{[this] { return monoMs; }, [this] { return epochMs; }}),
        store(kGateEnterM, kGateExitM, kConfirmHits, kTimeoutMs, log,
              [this] { return epochMs; }, [this] { return monoMs; }) {}
};

// (1) lastUpdated stepped forward AND backward, monotonic stamp held:
//     no track expires. Expiry never reads the realtime domain.
TEST(ExpiryMonotonic, RealtimeStepsWithMonotonicHeldDoNotExpire) {
  Harness h;
  h.store.apply(MakeObject("v2x:1001:7", Source::v2x_relayed, 25.0));
  ASSERT_EQ(h.store.get("v2x:1001:7")->timestamps.lastUpdated, kEpoch0);

  // Forward step: the wall clock jumps an hour ahead; the monotonic clock is
  // held, so the refresh restamps lastUpdated far forward while the per-track
  // monotonic stamp keeps its value.
  h.epochMs = kEpoch0 + kClockStepMs;
  h.store.apply(MakeObject("v2x:1001:7", Source::v2x_relayed, 25.0));
  ASSERT_EQ(h.store.get("v2x:1001:7")->timestamps.lastUpdated, kEpoch0 + kClockStepMs);
  EXPECT_EQ(h.store.expire(kMono0 + kTimeoutMs), 0u);
  EXPECT_TRUE(h.store.get("v2x:1001:7").has_value());

  // Backward step: the wall clock jumps an hour behind its origin. If expiry
  // compared against lastUpdated, now − lastUpdated would blow past the
  // timeout; on the monotonic stamp nothing expires.
  h.epochMs = kEpoch0 - kClockStepMs;
  h.store.apply(MakeObject("v2x:1001:7", Source::v2x_relayed, 25.0));
  ASSERT_EQ(h.store.get("v2x:1001:7")->timestamps.lastUpdated, kEpoch0 - kClockStepMs);
  EXPECT_EQ(h.store.expire(kMono0 + kTimeoutMs), 0u);
  EXPECT_TRUE(h.store.get("v2x:1001:7").has_value());

  // The whole run produced one admit transition and no expiry evidence.
  EXPECT_EQ(h.log.count(events::kTrackTransition), 1u);
  EXPECT_EQ(h.log.count(events::kTrackExpire), 0u);
}

// (2) Monotonic time advanced past TRACK_TIMEOUT_MS, lastUpdated untouched:
//     the track is erased.
TEST(ExpiryMonotonic, MonotonicSilenceAloneErasesTheTrack) {
  Harness h;
  h.store.apply(MakeObject("v2x:1001:7", Source::v2x_relayed, 25.0));
  ASSERT_EQ(h.store.get("v2x:1001:7")->timestamps.lastUpdated, kEpoch0);

  // Silence: no further updates, so lastUpdated stays untouched at kEpoch0.
  // The injected monotonic time alone crosses the timeout.
  EXPECT_EQ(h.store.expire(kMono0 + kTimeoutMs), 0u);      // exactly at: strict >
  ASSERT_EQ(h.store.expire(kMono0 + kTimeoutMs + 1), 1u);  // past: erased

  EXPECT_FALSE(h.store.get("v2x:1001:7").has_value());
  EXPECT_TRUE(h.store.empty());
  EXPECT_EQ(h.log.count(events::kTrackExpire), 1u);
  EXPECT_EQ(h.log.count(events::kTrackTransition), 2u);  // admit + timeout
}

// expire(now) visits EVERY track (D2's fusion tick): only the silent one
// goes, whatever its source.
TEST(ExpiryMonotonic, ExpireVisitsEveryTrackAndErasesOnlyTheSilentOne) {
  Harness h;
  h.store.apply(MakeObject("v2x:1001:7", Source::v2x_relayed, 25.0));  // stamped kMono0
  h.monoMs = kMono0 + 800;
  h.store.apply(MakeObject("own:1", Source::own_sensor, 20.0));  // stamped kMono0 + 800

  // At kMono0 + 1001 the relayed track's silence is 1001 ms (> timeout), the
  // own-sensor track's is 201 ms.
  ASSERT_EQ(h.store.expire(kMono0 + kTimeoutMs + 1), 1u);
  EXPECT_FALSE(h.store.get("v2x:1001:7").has_value());
  ASSERT_TRUE(h.store.get("own:1").has_value());
  EXPECT_EQ(h.store.size(), 1u);

  // The expiry evidence names the erased track, not the survivor.
  std::istringstream stream(h.sink.str());
  std::string line;
  const std::string prefix = "[EVT] ";
  bool sawExpire = false;
  while (std::getline(stream, line)) {
    const nlohmann::json j = nlohmann::json::parse(line.substr(prefix.size()));
    if (j.at("event") != events::kTrackExpire) {
      continue;
    }
    sawExpire = true;
    EXPECT_EQ(j.at("payload").at("id"), "v2x:1001:7");
    EXPECT_EQ(j.at("payload").at("source"), "v2x_relayed");
  }
  EXPECT_TRUE(sawExpire);
  EXPECT_EQ(h.log.count(events::kTrackExpire), 1u);
}

}  // namespace
