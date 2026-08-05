#include "parser/r3_parser.hpp"

#include <cstddef>
#include <utility>

#include <nlohmann/json.hpp>

namespace ada::parser {

namespace {

bool IsBlank(const std::string& s) {
  return s.find_first_not_of(" \t\r\n") == std::string::npos;
}

// Range bounds fixed by the frozen R3 schema
// (ADA_ECU/contracts/r3-tracked-object.schema.json) — contract constants,
// not tunables.
constexpr double kConfidenceMin = 0.0;
constexpr double kConfidenceMax = 1.0;
constexpr double kDistanceMin = 0.0;
constexpr double kSpeedMin = 0.0;

}  // namespace

R3ParseResult R3Parser::reject(R3RejectReason reason) {
  ++rejected_[static_cast<std::size_t>(reason)];
  R3ParseResult result;
  result.reason = reason;
  return result;
}

R3ParseResult R3Parser::parse(const std::string& line) {
  if (IsBlank(line)) {
    return reject(R3RejectReason::EmptyInput);
  }

  nlohmann::json doc;
  try {
    doc = nlohmann::json::parse(line);
  } catch (const nlohmann::json::parse_error&) {
    return reject(R3RejectReason::MalformedJson);
  }
  if (!doc.is_object()) {
    return reject(R3RejectReason::MalformedJson);
  }

  // The frozen binding is the only reader of the document. It throws
  // out_of_range on a missing required key and type_error on a wrong-typed
  // one; unknown extra keys it ignores (additive evolution).
  contracts::TrackedObject obj;
  try {
    obj = doc.get<contracts::TrackedObject>();
  } catch (const nlohmann::json::out_of_range&) {
    return reject(R3RejectReason::MissingField);
  } catch (const nlohmann::json::type_error&) {
    return reject(R3RejectReason::WrongFieldType);
  }

  // A detector line cannot mint relayed entries (D6). Checked on the parsed
  // enum, not the raw document: the binding maps any unknown source string to
  // own_sensor (its first enum entry), so nothing a detector emits can ever
  // land in the store as v2x_relayed.
  if (obj.source != contracts::Source::own_sensor) {
    return reject(R3RejectReason::NonOwnSensorSource);
  }

  if (obj.confidence < kConfidenceMin || obj.confidence > kConfidenceMax) {
    return reject(R3RejectReason::OutOfRangeValue);
  }
  if (obj.distance < kDistanceMin) {
    return reject(R3RejectReason::OutOfRangeValue);
  }
  if (obj.speed < kSpeedMin) {
    return reject(R3RejectReason::OutOfRangeValue);
  }

  // The incoming state is ignored — the store is the sole writer (D3).
  obj.state = contracts::TrackState::not_tracked;

  R3ParseResult result;
  result.object = std::move(obj);
  ++accepted_;
  return result;
}

}  // namespace ada::parser
