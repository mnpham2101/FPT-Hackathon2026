#ifndef ADA_ECU_LOG_EVENT_LOG_HPP
#define ADA_ECU_LOG_EVENT_LOG_HPP

// The R18 [EVT] JSONL writer — the ADA half of the evidence stream (HLD D8).
// One line per event: [EVT] {"event":…,"mono_ms":…,"epoch_ms":…,
// "counters":{…},"payload":{…}}, flushed per line. The prefix, the per-line
// flush and the clock-stamp pair match the V2X ECU's shape, so one offline
// reader walks both nodes' streams. Field names freeze here — later checkers
// (check_evt_log.py, event_report.py) parse them.

#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace ada::log {

// The D8 event vocabulary — all twelve declared; Phase 2 emits the first
// eight, the last four are first written in Phase 4.
namespace events {
inline constexpr const char* kDetectorSpawn = "detector_spawn";
inline constexpr const char* kDetectorEof = "detector_eof";
inline constexpr const char* kDetectorRestart = "detector_restart";
inline constexpr const char* kOwnSensorIngest = "own_sensor_ingest";
inline constexpr const char* kR2Ingest = "r2_ingest";
inline constexpr const char* kParseReject = "parse_reject";
inline constexpr const char* kTrackTransition = "track_transition";
inline constexpr const char* kTrackExpire = "track_expire";
inline constexpr const char* kAssessment = "assessment";           // Phase 4
inline constexpr const char* kAssessSkippedBUnknown = "assess_skipped_b_unknown";  // Phase 4
inline constexpr const char* kRiskTransition = "risk_transition";  // Phase 4
inline constexpr const char* kR4Tx = "r4_tx";                      // Phase 4

inline constexpr const char* kAll[] = {
    kDetectorSpawn,   kDetectorEof,   kDetectorRestart, kOwnSensorIngest,
    kR2Ingest,        kParseReject,   kTrackTransition, kTrackExpire,
    kAssessment,      kAssessSkippedBUnknown,           kRiskTransition,
    kR4Tx,
};
}  // namespace events

class EventLog {
 public:
  // Injectable clocks so tests get deterministic stamps. Both stamps are
  // written on every line, never derived from each other:
  //   mono_ms  — CLOCK_MONOTONIC (steady_clock), the interval clock
  //   epoch_ms — CLOCK_REALTIME (system_clock), the wire/log clock
  struct Clocks {
    std::function<std::int64_t()> monoMs;
    std::function<std::int64_t()> epochMs;
  };

  // filePath: additionally append to this file when non-empty (EVENT_LOG_PATH;
  //           empty = stdout only). Throws std::runtime_error if unopenable.
  // sink:     the live stream; nullptr means std::cout (tests inject one).
  // clocks:   empty functions fall back to the real clocks above.
  explicit EventLog(std::string filePath = std::string(), std::ostream* sink = nullptr,
                    Clocks clocks = Clocks{});

  // Writes one [EVT] JSONL line carrying the event name, both clock stamps,
  // the cumulative per-event counters (this event already counted), and the
  // payload. Serialization is nlohmann-only. Thread-safe.
  void emit(const std::string& event, nlohmann::json payload = nlohmann::json::object());

  // Cumulative count of one event name; 0 when never emitted.
  std::uint64_t count(const std::string& event) const;

 private:
  mutable std::mutex mutex_;
  std::ostream* sink_;
  std::ofstream file_;
  Clocks clocks_;
  std::map<std::string, std::uint64_t> counters_;
};

}  // namespace ada::log

#endif  // ADA_ECU_LOG_EVENT_LOG_HPP
