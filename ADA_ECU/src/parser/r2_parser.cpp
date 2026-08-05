#include "parser/r2_parser.hpp"

#include <cstddef>
#include <utility>

#include <nlohmann/json.hpp>

#include "contracts/r2_message.hpp"

namespace ada::parser {

namespace {

bool IsBlank(const std::string& s) {
  return s.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

R2ParseResult R2Parser::reject(R2RejectReason reason) {
  ++rejected_[static_cast<std::size_t>(reason)];
  R2ParseResult result;
  result.reason = reason;
  return result;
}

R2ParseResult R2Parser::parse(const std::string& body) {
  if (IsBlank(body)) {
    return reject(R2RejectReason::EmptyInput);
  }

  nlohmann::json doc;
  try {
    doc = nlohmann::json::parse(body);
  } catch (const nlohmann::json::parse_error&) {
    return reject(R2RejectReason::MalformedJson);
  }
  if (!doc.is_object()) {
    return reject(R2RejectReason::MalformedJson);
  }

  // The frozen binding is the only reader of the document. It throws
  // out_of_range on a missing required key and type_error on a wrong-typed
  // one; unknown extra keys it ignores (additive evolution).
  contracts::R2Message msg;
  try {
    msg = doc.get<contracts::R2Message>();
  } catch (const nlohmann::json::out_of_range&) {
    return reject(R2RejectReason::MissingField);
  } catch (const nlohmann::json::type_error&) {
    return reject(R2RejectReason::WrongFieldType);
  }

  if (msg.type != "v2x_object") {
    return reject(R2RejectReason::WrongType);
  }

  contracts::TrackedObject obj;
  obj.id = "v2x:" + std::to_string(msg.stationId) + ":" + std::to_string(msg.object.objectId);
  obj.object_class = msg.object.classification;
  obj.source = contracts::Source::v2x_relayed;
  obj.position.x = msg.object.position.x;
  obj.position.y = msg.object.position.y;
  obj.distance = msg.object.distance;  // as received — never recomputed here (HLD §10.1)
  obj.speed = msg.object.speed;
  obj.confidence = msg.object.confidence.value_or(0.0);  // wire null → lowest confidence
  obj.state = contracts::TrackState::not_tracked;  // placeholder; store is the sole writer (D3)
  // One clock domain: both operands come from the same R2 message (D10).
  obj.timestamps.measured =
      msg.rxTime + static_cast<std::int64_t>(msg.object.timeOfMeasurement);
  obj.timestamps.received = msg.rxTime;  // the V2X ECU's clock, a record value (D10)
  obj.timestamps.lastUpdated = 0;  // placeholder; the store stamps its own clock (§10.2)

  R2ParseResult result;
  result.receivedObjectConfidence = msg.object.confidence;
  result.positionConfidence = msg.object.position.confidence;
  result.object = std::move(obj);
  ++accepted_;
  return result;
}

}  // namespace ada::parser
