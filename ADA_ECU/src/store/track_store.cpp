// Implementation of the R3 track store — see track_store.hpp for the design
// notes (HLD §6 store/track_store row, D2 single-writer, D3 state ownership).

#include "store/track_store.hpp"

#include <chrono>
#include <utility>

namespace ada::store {

namespace {

// CLOCK_REALTIME, milliseconds since epoch (system_clock is the realtime
// clock on the target platform) — the fallback when no clock is injected.
std::int64_t RealtimeEpochMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

TrackStore::TrackStore(EpochClock epochMs) : epochMs_(std::move(epochMs)) {}

const contracts::TrackedObject& TrackStore::upsert(const contracts::TrackedObject& object) {
  const std::int64_t now = epochMs_ ? epochMs_() : RealtimeEpochMs();

  auto it = tracks_.find(object.id);
  if (it == tracks_.end()) {
    // First insert: every field from the incoming object, except state —
    // the store owns it (D3), and a new entry starts not_tracked.
    it = tracks_.emplace(object.id, object).first;
    it->second.state = contracts::TrackState::not_tracked;
  } else {
    // Refresh: every field from the incoming object, except state — the
    // stored value is preserved, never taken from the incoming object (D3).
    const contracts::TrackState stored = it->second.state;
    it->second = object;
    it->second.state = stored;
  }

  // The stamp is the store's, not the parser's: whatever lastUpdated the
  // incoming object carried is discarded.
  it->second.timestamps.lastUpdated = now;
  return it->second;
}

std::optional<contracts::TrackedObject> TrackStore::get(const std::string& id) const {
  const auto it = tracks_.find(id);
  if (it == tracks_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<contracts::TrackedObject> TrackStore::all() const {
  std::vector<contracts::TrackedObject> out;
  out.reserve(tracks_.size());
  for (const auto& entry : tracks_) {
    out.push_back(entry.second);
  }
  return out;
}

std::vector<contracts::TrackedObject> TrackStore::allBySource(contracts::Source source) const {
  std::vector<contracts::TrackedObject> out;
  for (const auto& entry : tracks_) {
    if (entry.second.source == source) {
      out.push_back(entry.second);
    }
  }
  return out;
}

std::optional<contracts::TrackedObject> TrackStore::nearest(contracts::Source source) const {
  const contracts::TrackedObject* best = nullptr;
  for (const auto& entry : tracks_) {
    if (entry.second.source != source) {
      continue;
    }
    // Strict < keeps the first (smallest-id) entry on an exact distance tie,
    // because std::map iterates in id order.
    if (best == nullptr || entry.second.distance < best->distance) {
      best = &entry.second;
    }
  }
  if (best == nullptr) {
    return std::nullopt;
  }
  return *best;
}

bool TrackStore::erase(const std::string& id) { return tracks_.erase(id) > 0; }

}  // namespace ada::store
