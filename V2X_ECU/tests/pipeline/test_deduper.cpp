// Deduper test (subtask 9.1.4.2) — stage 3 of the R9 Rx pipeline (Phase 1
// HLD §3 D3): duplicate drop over (stationId, objectId,
// referenceTime + measurementDeltaTime) within a sliding window.
//
// The clock is a mutable int64 captured by a lambda, so every window edge is
// exercised deterministically. The window length is a test-local literal —
// production code takes it injected (Config::dedupe_window; the default lives
// in config.cpp alone).

#include "pipeline/deduper.hpp"

#include <chrono>
#include <cstdint>

#include <gtest/gtest.h>

namespace {

using std::chrono::milliseconds;
using v2x::codec::CpmContent;
using v2x::pipeline::Deduper;

constexpr std::int64_t kWindowMs = 1500;  // test-local; NOT a production default

// Minimal content carrying just the key fields; the non-key fields keep their
// zero-initialized values — the deduper must ignore them.
CpmContent makeContent(std::uint32_t stationId, std::uint16_t objectId,
                       std::uint64_t referenceTime,
                       std::int16_t measurementDeltaTime) {
  CpmContent content{};
  content.stationId = stationId;
  content.object.objectId = objectId;
  content.referenceTime = referenceTime;
  content.object.measurementDeltaTime = measurementDeltaTime;
  return content;
}

class DeduperTest : public ::testing::Test {
 protected:
  std::int64_t clock_ms_ = 0;
  Deduper deduper_{milliseconds(kWindowMs), [this] { return clock_ms_; }};
};

// (a) Same key re-seen strictly inside the window drops.
TEST_F(DeduperTest, SameKeyInsideWindowDrops) {
  const auto content = makeContent(7, 42, 1000, 5);

  EXPECT_FALSE(deduper_.shouldDrop(content));  // first sighting passes

  clock_ms_ += kWindowMs - 1;
  EXPECT_TRUE(deduper_.shouldDrop(content));  // window−1: still a duplicate
}

// (b) Same key at exactly t+window (and beyond) passes — and REFRESHES the
// entry, so the window restarts from the re-sighting.
TEST_F(DeduperTest, SameKeyAtWindowPassesAndRefreshes) {
  const auto content = makeContent(7, 42, 1000, 5);

  EXPECT_FALSE(deduper_.shouldDrop(content));

  clock_ms_ += kWindowMs;
  EXPECT_FALSE(deduper_.shouldDrop(content));  // exactly window: passes

  clock_ms_ += kWindowMs - 1;
  EXPECT_TRUE(deduper_.shouldDrop(content))
      << "entry was not refreshed by the re-sighting at t+window";
}

TEST_F(DeduperTest, SameKeyBeyondWindowPasses) {
  const auto content = makeContent(7, 42, 1000, 5);

  EXPECT_FALSE(deduper_.shouldDrop(content));

  clock_ms_ += kWindowMs + 1;
  EXPECT_FALSE(deduper_.shouldDrop(content));
}

// (c) Any differing key component makes a distinct key — all pass back-to-back.
TEST_F(DeduperTest, DifferingStationIdPasses) {
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000, 5)));
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(8, 42, 1000, 5)));
}

TEST_F(DeduperTest, DifferingObjectIdPasses) {
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000, 5)));
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 43, 1000, 5)));
}

TEST_F(DeduperTest, DifferingTimestampSumPasses) {
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000, 5)));   // sum 1005
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000, 6)));   // sum 1006
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1002, 5)));   // sum 1007
}

// (d) Key identity is the SIGNED SUM referenceTime + measurementDeltaTime:
// differing parts with an equal sum are the same measurement → duplicate.
TEST_F(DeduperTest, EqualTimestampSumCollides) {
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000, 5)));  // sum 1005
  EXPECT_TRUE(deduper_.shouldDrop(makeContent(7, 42, 995, 10)));   // sum 1005 → duplicate
}

// Negative mdt participates in the signed sum the same way.
TEST_F(DeduperTest, NegativeMdtSumCollides) {
  EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000, -5)));  // sum 995
  EXPECT_TRUE(deduper_.shouldDrop(makeContent(7, 42, 990, 5)));     // sum 995 → duplicate
}

// (e) Pruning keeps the live set bounded: expired entries are swept, so size()
// does not grow monotonically past the keys of the last two windows.
TEST_F(DeduperTest, PruningBoundsSize) {
  constexpr int kFirstBatch = 100;
  constexpr int kSecondBatch = 50;

  for (int i = 0; i < kFirstBatch; ++i) {
    EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 1000 + i, 0)));
  }
  EXPECT_EQ(deduper_.size(), static_cast<std::size_t>(kFirstBatch));

  clock_ms_ += kWindowMs + 1;  // the whole first batch is now expired

  for (int i = 0; i < kSecondBatch; ++i) {
    EXPECT_FALSE(deduper_.shouldDrop(makeContent(7, 42, 5000 + i, 0)));
    // Never past first + second: the periodic sweep must have fired by now.
    EXPECT_LE(deduper_.size(),
              static_cast<std::size_t>(kFirstBatch + kSecondBatch));
  }

  // The sweep on the first post-expiry call removed the entire first batch —
  // only the live second batch remains.
  EXPECT_EQ(deduper_.size(), static_cast<std::size_t>(kSecondBatch));
}

}  // namespace
