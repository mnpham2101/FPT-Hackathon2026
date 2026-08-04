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
// at write, discarding whatever the parser left there. No admission logic
// lives here — that is store/admission, a separate component (D3).
//
// Single-writer by design (D2): only the main thread touches the store, so
// there is no lock. No I/O, no logging, no sockets inside (house rules).

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "contracts/tracked_object.hpp"

namespace ada::store {

// Injectable CLOCK_REALTIME reader, milliseconds since epoch, so tests get
// deterministic lastUpdated stamps (the src/config EnvGetter / src/log Clocks
// pattern). An empty function falls back to the real system_clock.
using EpochClock = std::function<std::int64_t()>;

class TrackStore {
 public:
  explicit TrackStore(EpochClock epochMs = EpochClock{});

  // The identical entry point for both parsers. Inserts or refreshes the
  // entry keyed by object.id:
  //   - state: preserved from the stored entry; not_tracked on first insert.
  //     The incoming object's state is ignored (D3).
  //   - timestamps.lastUpdated: overwritten with the injected clock's value.
  //   - every other field: taken from the incoming object.
  // Returns the entry as stored.
  const contracts::TrackedObject& upsert(const contracts::TrackedObject& object);

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
  // Returns false when the id was not present.
  bool erase(const std::string& id);

  std::size_t size() const { return tracks_.size(); }
  bool empty() const { return tracks_.empty(); }

 private:
  EpochClock epochMs_;
  std::map<std::string, contracts::TrackedObject> tracks_;
};

}  // namespace ada::store

#endif  // ADA_ECU_STORE_TRACK_STORE_HPP
