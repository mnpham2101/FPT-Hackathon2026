// V2X_ECU/src/pipeline/deduper.cpp — stage 3 of the R9 Rx pipeline (Phase 1
// HLD §3 D3). Semantics documented in deduper.hpp.

#include "pipeline/deduper.hpp"

#include <utility>

namespace v2x::pipeline {

namespace {

// Default clock: monotonic milliseconds from steady_clock. Wall-clock jumps
// (NTP steps) must neither open nor close the dedupe window, so system_clock
// is deliberately not used.
std::int64_t steadyNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

Deduper::Deduper(std::chrono::milliseconds window,
                 std::function<std::int64_t()> now_ms)
    : window_(window),
      now_ms_(now_ms ? std::move(now_ms)
                     : std::function<std::int64_t()>(steadyNowMs)),
      last_prune_(now_ms_()) {}

bool Deduper::shouldDrop(const v2x::codec::CpmContent& content) {
  const std::int64_t now = now_ms_();

  // Signed sum: referenceTime is uint64 ms since the ITS epoch (well within
  // int64), measurementDeltaTime is int16 ms — the sum is the object's time
  // of measurement, and it is the sum (not the parts) that identifies it.
  const Key key{content.stationId, content.object.objectId,
                static_cast<std::int64_t>(content.referenceTime) +
                    content.object.measurementDeltaTime};

  prune(now);

  const auto it = seen_.find(key);
  if (it != seen_.end() && now - it->second < window_.count()) {
    return true;  // duplicate inside the window — entry left untouched
  }

  // First sighting, or a re-sighting at/after the window: record/refresh so
  // the window restarts from this sighting (deduper.hpp header comment).
  seen_[key] = now;
  return false;
}

void Deduper::prune(std::int64_t now) {
  if (now - last_prune_ < window_.count()) {
    return;  // at most one sweep per window — O(1) amortized
  }
  last_prune_ = now;
  for (auto it = seen_.begin(); it != seen_.end();) {
    if (now - it->second >= window_.count()) {
      it = seen_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace v2x::pipeline
