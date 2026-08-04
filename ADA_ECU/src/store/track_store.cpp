// Implementation of the R3 track store — see track_store.hpp for the design
// notes (HLD §6 store/track_store row, D2 single-writer, D3 state ownership,
// D10 clock domains) and the 13.2.4.3 admission wiring it hosts.

#include "store/track_store.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "log/event_log.hpp"

namespace ada::store {

namespace {

// CLOCK_REALTIME, milliseconds since epoch (system_clock is the realtime
// clock on the target platform) — the fallback when no clock is injected.
std::int64_t RealtimeEpochMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// CLOCK_MONOTONIC, milliseconds (steady_clock on the target platform) — the
// fallback when no mono clock is injected.
std::int64_t MonotonicMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// The five state-changing edges emit a track_transition; the two self-loops
// (counted, refreshed) and none do not — the 13.2.4.3 ruling.
bool IsStateChanging(AdmissionEdge edge) {
  switch (edge) {
    case AdmissionEdge::gate_enter:
    case AdmissionEdge::confirmed:
    case AdmissionEdge::out_of_gate:
    case AdmissionEdge::gate_exit:
    case AdmissionEdge::timeout:
      return true;
    case AdmissionEdge::counted:
    case AdmissionEdge::refreshed:
    case AdmissionEdge::none:
      break;
  }
  return false;
}

}  // namespace

TrackStore::TrackStore(EpochClock epochMs, MonoClock monoMs)
    : epochMs_(std::move(epochMs)), monoMs_(std::move(monoMs)) {}

TrackStore::TrackStore(double gateEnterM, double gateExitM, int confirmHits,
                       std::int64_t trackTimeoutMs, log::EventLog& eventLog,
                       EpochClock epochMs, MonoClock monoMs)
    : epochMs_(std::move(epochMs)),
      monoMs_(std::move(monoMs)),
      machine_(std::in_place, gateEnterM, gateExitM, confirmHits, trackTimeoutMs),
      eventLog_(&eventLog) {}

std::int64_t TrackStore::epochNow() const { return epochMs_ ? epochMs_() : RealtimeEpochMs(); }

std::int64_t TrackStore::monoNow() const { return monoMs_ ? monoMs_() : MonotonicMs(); }

void TrackStore::requireWired(const char* method) const {
  if (!machine_.has_value() || eventLog_ == nullptr) {
    throw std::logic_error(std::string("TrackStore::") + method +
                           " requires the admission-wired constructor");
  }
}

TrackStore::Entry& TrackStore::upsertEntry(const contracts::TrackedObject& object) {
  auto it = tracks_.find(object.id);
  if (it == tracks_.end()) {
    // First insert: every field from the incoming object, except state —
    // the store owns it (D3), and a new entry starts not_tracked.
    it = tracks_.emplace(object.id, Entry{object, 0, 0}).first;
    it->second.object.state = contracts::TrackState::not_tracked;
  } else {
    // Refresh: every field from the incoming object, except state — the
    // stored value is preserved, never taken from the incoming object (D3).
    // The hits counter is the store's and survives the refresh untouched.
    const contracts::TrackState stored = it->second.object.state;
    it->second.object = object;
    it->second.object.state = stored;
  }

  // Both stamps are the store's, not the parser's: whatever lastUpdated the
  // incoming object carried is discarded, and the parallel monotonic stamp
  // (D10) is restamped on every observation.
  it->second.object.timestamps.lastUpdated = epochNow();
  it->second.monoMs = monoNow();
  return it->second;
}

const contracts::TrackedObject& TrackStore::upsert(const contracts::TrackedObject& object) {
  return upsertEntry(object).object;
}

AdmissionResult TrackStore::apply(const contracts::TrackedObject& object) {
  requireWired("apply");
  const std::int64_t nowMono = monoNow();

  // The stimulus: stored state, hits and monotonic elapsed when the entry
  // exists; not_tracked / 0 / 0 when absent (admission.hpp's contract).
  AdmissionInput in;
  in.distanceM = object.distance;
  double lastDistance = object.distance;
  const auto it = tracks_.find(object.id);
  if (it != tracks_.end()) {
    in.state = it->second.object.state;
    in.hits = it->second.hits;
    in.elapsedMs = nowMono - it->second.monoMs;
    lastDistance = it->second.object.distance;
  }

  const AdmissionResult r = machine_->step(in);

  switch (r.action) {
    case AdmissionAction::insert:
    case AdmissionAction::keep: {
      Entry& entry = upsertEntry(object);
      entry.object.state = r.state;
      entry.hits = r.hits;
      break;
    }
    case AdmissionAction::erase:
      tracks_.erase(object.id);
      break;
    case AdmissionAction::none:
      break;
  }

  // The timeout edge fired on a stale update: the observation was not
  // admitted, so the event carries the last accepted distance; every other
  // edge carries the observation's own distance.
  const double eventDistance = r.edge == AdmissionEdge::timeout ? lastDistance : object.distance;
  emitTransition(object.id, object.source, in.state, r.state, eventDistance, r.edge);
  if (r.edge == AdmissionEdge::timeout) {
    emitExpire(object.id, object.source, eventDistance);
  }
  return r;
}

std::size_t TrackStore::expire(std::int64_t nowMonoMs) {
  requireWired("expire");
  std::size_t erased = 0;
  for (auto it = tracks_.begin(); it != tracks_.end();) {
    AdmissionInput in;
    in.state = it->second.object.state;
    in.hits = it->second.hits;
    in.distanceM = std::nullopt;  // expiry tick: only the timeout edge can fire
    in.elapsedMs = nowMonoMs - it->second.monoMs;

    const AdmissionResult r = machine_->step(in);
    if (r.action != AdmissionAction::erase) {
      ++it;  // inside the timeout — the entry is untouched
      continue;
    }

    const std::string id = it->first;
    const contracts::Source source = it->second.object.source;
    const contracts::TrackState from = it->second.object.state;
    const double lastDistance = it->second.object.distance;
    it = tracks_.erase(it);
    ++erased;
    emitTransition(id, source, from, r.state, lastDistance, r.edge);
    emitExpire(id, source, lastDistance);
  }
  return erased;
}

void TrackStore::emitTransition(const std::string& id, contracts::Source source,
                                contracts::TrackState from, contracts::TrackState to,
                                double distance, AdmissionEdge edge) {
  if (!IsStateChanging(edge)) {
    return;
  }
  // Payload field names are frozen by log/event_log's stream (18.2.2.3):
  // id, source, from, to, distance, reason. The enums serialize to their
  // wire strings via the contracts' NLOHMANN_JSON_SERIALIZE_ENUM.
  eventLog_->emit(log::events::kTrackTransition,
                  nlohmann::json{{"id", id},
                                 {"source", source},
                                 {"from", from},
                                 {"to", to},
                                 {"distance", distance},
                                 {"reason", edgeName(edge)}});
}

void TrackStore::emitExpire(const std::string& id, contracts::Source source, double distance) {
  eventLog_->emit(log::events::kTrackExpire,
                  nlohmann::json{{"id", id}, {"source", source}, {"distance", distance}});
}

std::optional<contracts::TrackedObject> TrackStore::get(const std::string& id) const {
  const auto it = tracks_.find(id);
  if (it == tracks_.end()) {
    return std::nullopt;
  }
  return it->second.object;
}

std::vector<contracts::TrackedObject> TrackStore::all() const {
  std::vector<contracts::TrackedObject> out;
  out.reserve(tracks_.size());
  for (const auto& entry : tracks_) {
    out.push_back(entry.second.object);
  }
  return out;
}

std::vector<contracts::TrackedObject> TrackStore::allBySource(contracts::Source source) const {
  std::vector<contracts::TrackedObject> out;
  for (const auto& entry : tracks_) {
    if (entry.second.object.source == source) {
      out.push_back(entry.second.object);
    }
  }
  return out;
}

std::optional<contracts::TrackedObject> TrackStore::nearest(contracts::Source source) const {
  const contracts::TrackedObject* best = nullptr;
  for (const auto& entry : tracks_) {
    if (entry.second.object.source != source) {
      continue;
    }
    // Strict < keeps the first (smallest-id) entry on an exact distance tie,
    // because std::map iterates in id order.
    if (best == nullptr || entry.second.object.distance < best->distance) {
      best = &entry.second.object;
    }
  }
  if (best == nullptr) {
    return std::nullopt;
  }
  return *best;
}

bool TrackStore::erase(const std::string& id) { return tracks_.erase(id) > 0; }

}  // namespace ada::store
