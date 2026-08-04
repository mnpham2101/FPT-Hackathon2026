#ifndef ADA_ECU_STORE_TRACK_STORE_HPP
#define ADA_ECU_STORE_TRACK_STORE_HPP

// The R3 track store (HLD §6, Data table, the store/track_store row) — the
// figure's Current Input: an id → TrackedObject map exposing every R3 field.
// Both parsers enter through the IDENTICAL upsert(), so a detector-shaped and
// a relayed-shaped entry are indistinguishable here except by `source` — the
// R3 acceptance box.
//
// The store is the sole writer of `state` (D3): upsert preserves the stored
// state and never takes it from the incoming object; a first insert enters as
// not_tracked. upsert also stamps timestamps.lastUpdated from CLOCK_REALTIME
// at write, discarding whatever the parser left there, and keeps a PARALLEL
// CLOCK_MONOTONIC stamp per entry (D10) — the only operand expiry compares.
// lastUpdated is a log/wire value and is never an interval operand: a host
// NTP step on the wall clock must not expire every track at once.
//
// The R13 rules stay in store/admission (a pure machine, D3); this class is
// its caller — the wiring 13.2.4.3 adds. Constructed with the admission
// thresholds and an EventLog, apply() runs the machine on every ingest and
// expire() runs the timeout edge for every track on the fusion tick, so a
// track can expire on silence alone (D2). Erasure removes the entry from the
// map — not_tracked is absence, never a flag (D3). Each state-changing edge
// (gate_enter, confirmed, out_of_gate, gate_exit, timeout) emits exactly one
// track_transition through the injected EventLog; the self-loop edges
// (counted, refreshed) update the track and emit nothing; the timeout edge
// additionally emits track_expire. The EventLog is the store's one logging
// seam — no other I/O, no sockets inside (house rules).
//
// Single-writer by design (D2): only the main thread touches the store, so
// there is no lock.

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "contracts/tracked_object.hpp"
#include "store/admission.hpp"

namespace ada::log {
class EventLog;  // full type only needed by track_store.cpp
}

namespace ada::store {

// Injectable CLOCK_REALTIME reader, milliseconds since epoch, so tests get
// deterministic lastUpdated stamps (the src/config EnvGetter / src/log Clocks
// pattern). An empty function falls back to the real system_clock.
using EpochClock = std::function<std::int64_t()>;

// Injectable CLOCK_MONOTONIC reader, milliseconds — the per-entry stamp
// expiry compares (D10). An empty function falls back to the real
// steady_clock.
using MonoClock = std::function<std::int64_t()>;

class TrackStore {
 public:
  // Data-only store: the map, no admission, no events. apply()/expire()
  // require the wired constructor below and throw std::logic_error here.
  explicit TrackStore(EpochClock epochMs = EpochClock{}, MonoClock monoMs = MonoClock{});

  // Admission-wired store (13.2.4.3). The thresholds are the machine's
  // constructor parameters, taken from the loaded Config (gateEnterM =
  // GATE_ENTER_M, gateExitM = GATE_EXIT_M, confirmHits = CONFIRM_HITS,
  // trackTimeoutMs = TRACK_TIMEOUT_MS) — no literals here or in the caller.
  // eventLog must outlive the store.
  TrackStore(double gateEnterM, double gateExitM, int confirmHits,
             std::int64_t trackTimeoutMs, log::EventLog& eventLog,
             EpochClock epochMs = EpochClock{}, MonoClock monoMs = MonoClock{});

  // The identical entry point for both parsers. Inserts or refreshes the
  // entry keyed by object.id:
  //   - state: preserved from the stored entry; not_tracked on first insert.
  //     The incoming object's state is ignored (D3).
  //   - timestamps.lastUpdated: overwritten with the injected epoch clock.
  //   - the parallel monotonic stamp: overwritten with the injected mono
  //     clock (D10).
  //   - every other field: taken from the incoming object.
  // Returns the entry as stored. Runs no admission — the wired ingest path
  // is apply(), which calls this internally.
  const contracts::TrackedObject& upsert(const contracts::TrackedObject& object);

  // Wired ingest (one update, D3's definition): reads the stored state, hits
  // and monotonic elapsed for object.id, steps the admission machine with
  // object.distance, applies the returned action (insert / keep via upsert,
  // erase by removal), and emits the events described in the header comment.
  // Requires the admission-wired constructor.
  AdmissionResult apply(const contracts::TrackedObject& object);

  // The fusion tick (D2): runs the timeout edge for EVERY track against
  // nowMonoMs − the entry's monotonic stamp, so a track expires on silence
  // alone. nowMonoMs is read by the caller from the same CLOCK_MONOTONIC
  // domain as the injected mono clock. Each expired track emits one
  // track_transition (reason timeout) plus one track_expire, and is removed.
  // Returns the number of tracks erased. Requires the admission-wired
  // constructor.
  std::size_t expire(std::int64_t nowMonoMs);

  // The stored entry, or nullopt when the id is absent.
  std::optional<contracts::TrackedObject> get(const std::string& id) const;

  // Every stored entry, in id order (std::map iteration — deterministic).
  std::vector<contracts::TrackedObject> all() const;

  // Every stored entry of one source, in id order.
  std::vector<contracts::TrackedObject> allBySource(contracts::Source source) const;

  // The smallest-distance entry of one source — the composition input
  // (HLD §6: `nearest(source)` for composition). nullopt when the source has
  // no entries; on an exact distance tie the smallest id wins.
  std::optional<contracts::TrackedObject> nearest(contracts::Source source) const;

  // Removes the entry; not_tracked means absent from the store (D3).
  // Returns false when the id was not present. Emits nothing — the evented
  // erasures are apply()'s and expire()'s.
  bool erase(const std::string& id);

  std::size_t size() const { return tracks_.size(); }
  bool empty() const { return tracks_.empty(); }

 private:
  // One stored track: the R3 object, plus the two admission-side values that
  // are the store's and never the wire's — the confirm counter and the
  // monotonic stamp (D10).
  struct Entry {
    contracts::TrackedObject object;
    int hits = 0;
    std::int64_t monoMs = 0;
  };

  std::int64_t epochNow() const;
  std::int64_t monoNow() const;
  void requireWired(const char* method) const;
  Entry& upsertEntry(const contracts::TrackedObject& object);

  // One track_transition per state-changing edge; nothing for the self-loops
  // (counted, refreshed) and for none — the planner's ruling on the .puml
  // note, fixed by the 13.2.4.3 brief.
  void emitTransition(const std::string& id, contracts::Source source,
                      contracts::TrackState from, contracts::TrackState to,
                      double distance, AdmissionEdge edge);
  void emitExpire(const std::string& id, contracts::Source source, double distance);

  EpochClock epochMs_;
  MonoClock monoMs_;
  std::optional<AdmissionMachine> machine_;
  log::EventLog* eventLog_ = nullptr;
  std::map<std::string, Entry> tracks_;
};

}  // namespace ada::store

#endif  // ADA_ECU_STORE_TRACK_STORE_HPP
