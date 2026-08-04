#ifndef V2X_ECU_LOG_EVENT_LOG_HPP
#define V2X_ECU_LOG_EVENT_LOG_HPP

// R18 JSONL evidence-stream writer (subtask 18.1.2.3; HLD decision D4 as
// amended 2026-08-01). One line per event, `[EVT] {json}`, flushed per line —
// stdout is the always-on sink (CarSky View Log is the live window); when
// EVENT_LOG_PATH is non-empty the same lines are additionally appended there.
//
// FROZEN JSON field names — parsed by the D7 checker
// tools/comms_check/check_v2x_log.py (subtask 9.1.12.2); renaming any of
// these breaks that consumer:
//
//   every event:    "event"    (string, one of the 9 D4 names below)
//                   "mono_ms"  (int64, std::chrono::steady_clock ms)
//                   "epoch_ms" (int64, std::chrono::system_clock ms)
//                   "counters" (object) with exactly the keys
//                     "rx_datagram", "decode_ok", "decode_reject",
//                     "validate_reject", "dedupe_drop", "r2_forwarded"
//                     — cumulative uint64 per pipeline stage, incremented
//                     when the same-named event is emitted. The three stub
//                     events (stub_transition, fault_injected, recovery)
//                     increment nothing: they report, they are not a stage.
//   decode_ok:      "cpm" — the decoded v2x::codec::CpmContent as a JSON
//                   object (via its to_json).
//   r2_forwarded:   "r2" — the forwarded v2x::contracts::R2Message as a JSON
//                   object (via its to_json).
//
// Non-frozen extras (free text / diagnostics, not parsed by D7):
//   "detail" (string) — reject reasons, stub transition names, fault names.
//   "bytes"  (number) — datagram size in octets, on rx_datagram only.
//
// D4 event vocabulary: rx_datagram, decode_ok, decode_reject,
// validate_reject, dedupe_drop, r2_forwarded, stub_transition,
// fault_injected, recovery.
//
// Thread-safe: a mutex serialises emission (Rx thread + main both log).
// Transport-blind — no socket headers (tools/check_transport_imports.py).

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <ostream>
#include <string>

#include <nlohmann/json.hpp>

#include "codec/cpm_codec.hpp"
#include "contracts/r2_message.hpp"

namespace v2x::log {

class EventLog {
 public:
  // `out` is the always-on line sink (main passes std::cout; tests pass a
  // std::ostringstream). A non-empty `file_path` (EVENT_LOG_PATH, HLD §6)
  // additionally appends every line to that file, opened once here and owned
  // for the object's lifetime (RAII via the std::ofstream member).
  explicit EventLog(std::ostream& out, const std::string& file_path = {});

  EventLog(const EventLog&) = delete;
  EventLog& operator=(const EventLog&) = delete;

  void rxDatagram(std::size_t bytes);
  void decodeOk(const v2x::codec::CpmContent& cpm);
  void decodeReject(const std::string& reason);
  void validateReject(const std::string& reason);
  void dedupeDrop();
  void r2Forwarded(const v2x::contracts::R2Message& r2);
  void stubTransition(const std::string& detail);
  void faultInjected(const std::string& detail);
  void recovery(const std::string& detail);

 private:
  // Cumulative per-stage counters (frozen "counters" object keys above).
  struct Counters {
    std::uint64_t rx_datagram{};
    std::uint64_t decode_ok{};
    std::uint64_t decode_reject{};
    std::uint64_t validate_reject{};
    std::uint64_t dedupe_drop{};
    std::uint64_t r2_forwarded{};
  };

  // Takes the lock, bumps `counter` when non-null (stage events pass their
  // counter; the three stub events pass nullptr), stamps event name +
  // timestamps + counters snapshot into `fields`, and writes the `[EVT]`
  // line to both sinks, flushed.
  void emit(const char* event, nlohmann::json fields, std::uint64_t* counter);

  std::ostream& out_;
  std::ofstream file_;
  Counters counters_;
  std::mutex mutex_;
};

}  // namespace v2x::log

#endif  // V2X_ECU_LOG_EVENT_LOG_HPP
