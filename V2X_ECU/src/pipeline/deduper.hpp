#ifndef V2X_ECU_PIPELINE_DEDUPER_HPP
#define V2X_ECU_PIPELINE_DEDUPER_HPP

// V2X_ECU/src/pipeline/deduper.hpp — stage 3 of the R9 Rx pipeline (Phase 1
// HLD decision D3): duplicate drop over the key
//   (stationId, objectId, referenceTime + measurementDeltaTime)
// within a sliding window.
//
// The timestamp component is the SIGNED SUM referenceTime + measurementDeltaTime
// — the object's time of measurement — so two CPMs whose parts differ but whose
// sums are equal (e.g. refTime 1000 + mdt 5 and refTime 995 + mdt 10) carry the
// same measurement and ARE duplicates of each other.
//
// Window semantics: a key seen within the last `window` ms (strictly less than
// window; a repeat at exactly t+window passes) is a duplicate and drops.
// Re-seeing a key at or after the window PASSES and REFRESHES the entry — the
// new sighting restarts the window, so a message repeated every window+ε ms is
// forwarded every time, never starved by its own history.
//
// The window length is injected (main passes Config::dedupe_window, sourced
// from DEDUPE_WINDOW_MS; the default lives in config.cpp alone — governing
// principle 5, no literal window value here). The clock is injectable too, so
// tests are deterministic; the default is a steady_clock wrapper (monotonic —
// wall-clock jumps must not open or close the window).
//
// Stateful but transport-blind: no sockets (tools/check_transport_imports.py
// applies), no env reads, no logging, no counters — the pipeline counts drops
// via the R18 event log (HLD D4).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include "codec/cpm_codec.hpp"

namespace v2x::pipeline {

class Deduper {
 public:
  // `window`: sliding-window length; caller-supplied, never defaulted here.
  // `now_ms`: monotonic milliseconds source; an empty function selects the
  // built-in steady_clock wrapper.
  explicit Deduper(std::chrono::milliseconds window,
                   std::function<std::int64_t()> now_ms = {});

  // True  → duplicate: the same key was seen strictly less than `window` ms
  //         ago; the stored entry is left untouched.
  // False → first sighting (or a re-sighting at/after the window): the key is
  //         recorded/refreshed with the current clock value.
  bool shouldDrop(const v2x::codec::CpmContent& content);

  // Number of live (recorded, possibly expired-but-not-yet-pruned) entries.
  // Exposed so tests can assert that pruning keeps memory bounded.
  std::size_t size() const { return seen_.size(); }

 private:
  // Dedupe key. The timestamp component folds referenceTime and
  // measurementDeltaTime into their signed sum (see header comment).
  struct Key {
    std::uint32_t stationId;
    std::uint16_t objectId;
    std::int64_t measurementTime;  // signed: referenceTime + measurementDeltaTime

    bool operator==(const Key& other) const {
      return stationId == other.stationId && objectId == other.objectId &&
             measurementTime == other.measurementTime;
    }
  };

  struct KeyHash {
    std::size_t operator()(const Key& k) const {
      // Boost-style hash combine over the three components.
      std::size_t seed = std::hash<std::uint32_t>{}(k.stationId);
      seed ^= std::hash<std::uint16_t>{}(k.objectId) + 0x9e3779b97f4a7c15ULL +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::int64_t>{}(k.measurementTime) +
              0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  // Drop every entry whose window has elapsed. shouldDrop calls this at most
  // once per window (periodic sweep), so an expired entry lives at most one
  // extra window and the map is bounded by the keys of the last two windows —
  // bounded under a continuous 10 Hz stream, O(1) amortized per call.
  void prune(std::int64_t now);

  std::chrono::milliseconds window_;
  std::function<std::int64_t()> now_ms_;
  std::unordered_map<Key, std::int64_t, KeyHash> seen_;  // key → last-recorded ms
  std::int64_t last_prune_;  // clock value of the last sweep
};

}  // namespace v2x::pipeline

#endif  // V2X_ECU_PIPELINE_DEDUPER_HPP
