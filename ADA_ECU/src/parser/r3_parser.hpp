#ifndef ADA_ECU_PARSER_R3_PARSER_HPP
#define ADA_ECU_PARSER_R3_PARSER_HPP

// Detector JSONL → TrackedObject parser (HLD §6, the parser/r3_parser
// controller row; §10.2). Parses one detector stdout line THROUGH the frozen
// contracts::TrackedObject binding and emits an object with
// source = own_sensor. The incoming `state` is ignored — the store is the
// sole writer of state (D3). A line whose source is not own_sensor is
// rejected: a detector cannot mint relayed entries, the structural half of
// the zero-C argument (D6). Failures return a typed reject reason and are
// counted; nothing throws into the pipeline. No I/O, no logging, no sockets
// inside (house rules; the caller emits own_sensor_ingest / parse_reject).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "contracts/tracked_object.hpp"

namespace ada::parser {

// Why one detector line produced no TrackedObject. None means accepted.
enum class R3RejectReason {
  None = 0,            // accepted — never counted as a reject
  EmptyInput,          // empty or whitespace-only line
  MalformedJson,       // not parseable JSON, or the top level is not an object
  MissingField,        // a schema-required key is absent (binding out_of_range)
  WrongFieldType,      // a present key has the wrong JSON type (binding type_error)
  NonOwnSensorSource,  // source is not own_sensor — a detector cannot mint relayed entries (D6)
  OutOfRangeValue,     // a schema range bound violated: confidence outside 0..1,
                       // or negative distance / speed
};

inline constexpr std::size_t kR3RejectReasonCount = 7;

// Snake-case reason name for parse_reject event payloads and test messages.
inline const char* to_string(R3RejectReason reason) {
  switch (reason) {
    case R3RejectReason::None:
      return "none";
    case R3RejectReason::EmptyInput:
      return "empty_input";
    case R3RejectReason::MalformedJson:
      return "malformed_json";
    case R3RejectReason::MissingField:
      return "missing_field";
    case R3RejectReason::WrongFieldType:
      return "wrong_field_type";
    case R3RejectReason::NonOwnSensorSource:
      return "non_own_sensor_source";
    case R3RejectReason::OutOfRangeValue:
      return "out_of_range_value";
  }
  return "unknown";
}

// One parse outcome. On accept, `object` is set with source = own_sensor and
// state = not_tracked (the placeholder the store overwrites, D3).
struct R3ParseResult {
  std::optional<contracts::TrackedObject> object;
  R3RejectReason reason = R3RejectReason::None;

  bool accepted() const { return object.has_value(); }
};

// Stateless mapping plus accept/reject counters (the counters are what the
// own_sensor_ingest / parse_reject events report; no logging in here).
class R3Parser {
 public:
  R3ParseResult parse(const std::string& line);

  std::uint64_t acceptedCount() const { return accepted_; }

  // Total rejects across all reasons.
  std::uint64_t rejectedCount() const {
    std::uint64_t total = 0;
    for (const auto n : rejected_) {
      total += n;
    }
    return total;
  }

  std::uint64_t rejectedCount(R3RejectReason reason) const {
    return rejected_[static_cast<std::size_t>(reason)];
  }

 private:
  R3ParseResult reject(R3RejectReason reason);

  std::uint64_t accepted_ = 0;
  std::array<std::uint64_t, kR3RejectReasonCount> rejected_{};
};

}  // namespace ada::parser

#endif  // ADA_ECU_PARSER_R3_PARSER_HPP
