#ifndef ADA_ECU_PARSER_R2_PARSER_HPP
#define ADA_ECU_PARSER_R2_PARSER_HPP

// R2 → TrackedObject parser (HLD §6, the parser/r2_parser controller row;
// mapping rules HLD §10.1/§10.2). Parses one received R2 datagram body
// THROUGH the frozen contracts::R2Message binding — never raw JSON probing —
// and emits a contracts::TrackedObject with source = v2x_relayed.
// Failures return a typed reject reason and are counted; nothing throws into
// the pipeline. No I/O, no logging, no sockets inside (house rules; the
// caller emits the r2_ingest / parse_reject events).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "contracts/tracked_object.hpp"

namespace ada::parser {

// Why one R2 body produced no TrackedObject. None means accepted.
enum class R2RejectReason {
  None = 0,        // accepted — never counted as a reject
  EmptyInput,      // empty or whitespace-only datagram body
  MalformedJson,   // not parseable JSON, or the top level is not a JSON object
  WrongType,       // "type" is present and a string, but not "v2x_object"
  MissingField,    // a schema-required key is absent (binding out_of_range)
  WrongFieldType,  // a present key has the wrong JSON type (binding type_error)
};

inline constexpr std::size_t kR2RejectReasonCount = 6;

// Snake-case reason name for parse_reject event payloads and test messages.
inline const char* to_string(R2RejectReason reason) {
  switch (reason) {
    case R2RejectReason::None:
      return "none";
    case R2RejectReason::EmptyInput:
      return "empty_input";
    case R2RejectReason::MalformedJson:
      return "malformed_json";
    case R2RejectReason::WrongType:
      return "wrong_type";
    case R2RejectReason::MissingField:
      return "missing_field";
    case R2RejectReason::WrongFieldType:
      return "wrong_field_type";
  }
  return "unknown";
}

// One parse outcome. On accept, `object` is set and `reason` is None; the two
// out-of-band values below ride beside the object because neither has an R3
// home (HLD §10.1 obligation rows) — they exist for the r2_ingest payload.
struct R2ParseResult {
  std::optional<contracts::TrackedObject> object;
  R2RejectReason reason = R2RejectReason::None;

  // object.confidence exactly as received: nullopt = wire null. The mapped
  // TrackedObject carries 0.0 for a null, so the null stays visible here.
  std::optional<double> receivedObjectConfidence;

  // position.confidence — a position accuracy in METRES (R2 field F6, not a
  // probability); dropped from the track, recorded out-of-band.
  std::optional<double> positionConfidence;

  bool accepted() const { return object.has_value(); }
};

// Stateless mapping plus accept/reject counters (the counters are what the
// r2_ingest / parse_reject events report; no logging in here).
//
// Mapping (HLD §10.1/§10.2, D3, D10):
//   id       = "v2x:" + stationId + ":" + object.objectId
//   class    = object.classification · source = v2x_relayed
//   position = object.position{x,y} · distance = object.distance (as received,
//              never recomputed) · speed = object.speed
//   confidence = object.confidence, or 0.0 when the wire value is null
//   state    = not_tracked placeholder — the store is the sole writer (D3)
//   timestamps.measured    = rxTime + object.timeOfMeasurement (a −2048..2047
//                            ms delta; both operands from the same R2 message,
//                            so one clock domain — D10)
//   timestamps.received    = rxTime (the V2X ECU's clock, a record value —
//                            never an operand beyond the line above)
//   timestamps.lastUpdated = 0 placeholder — the store stamps its own
//                            CLOCK_REALTIME at write and never keeps a foreign
//                            node's value there (§10.2)
// The ADA-side receive stamp never enters the R3 object; it rides on
// InputItem.rxEpochMs for the r2_ingest event only.
class R2Parser {
 public:
  R2ParseResult parse(const std::string& body);

  std::uint64_t acceptedCount() const { return accepted_; }

  // Total rejects across all reasons.
  std::uint64_t rejectedCount() const {
    std::uint64_t total = 0;
    for (const auto n : rejected_) {
      total += n;
    }
    return total;
  }

  std::uint64_t rejectedCount(R2RejectReason reason) const {
    return rejected_[static_cast<std::size_t>(reason)];
  }

 private:
  R2ParseResult reject(R2RejectReason reason);

  std::uint64_t accepted_ = 0;
  std::array<std::uint64_t, kR2RejectReasonCount> rejected_{};
};

}  // namespace ada::parser

#endif  // ADA_ECU_PARSER_R2_PARSER_HPP
