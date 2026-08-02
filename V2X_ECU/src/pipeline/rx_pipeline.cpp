// V2X_ECU/src/pipeline/rx_pipeline.cpp — the R9 four-stage Rx pipeline.
// Contract, lifetime and transport-blindness rules: rx_pipeline.hpp.

#include "pipeline/rx_pipeline.hpp"

#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace v2x::pipeline {

namespace {

// Default rx-time source: wall-clock milliseconds since the Unix epoch, the
// unit R2Message::rxTime is defined in. Unlike the Deduper's window clock this
// one is deliberately NOT monotonic — rxTime is an epoch stamp consumers
// correlate against their own wall clocks.
std::int64_t systemNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Prefix marking a reject that came from an unexpected exception rather than a
// real decode failure — see the attribution note in onDatagram().
constexpr const char* kUnexpectedPrefix = "unexpected_exception: ";

}  // namespace

RxPipeline::RxPipeline(v2x::codec::ICpmCodec& codec, Validator& validator, Deduper& deduper,
                       R2Builder& builder, v2x::log::EventLog& log, R2Sink sink,
                       std::function<std::int64_t()> now_ms)
    : codec_(codec),
      validator_(validator),
      deduper_(deduper),
      builder_(builder),
      log_(log),
      sink_(std::move(sink)),
      now_ms_(now_ms ? std::move(now_ms)
                     : std::function<std::int64_t()>(systemNowMs)) {}

void RxPipeline::onDatagram(const std::vector<std::uint8_t>& bytes) noexcept {
  // EXCEPTION ATTRIBUTION (the choice this comment is required to state):
  // every unexpected throw from any stage is reported as ONE `decode_reject`
  // carrying an "unexpected_exception: ..." detail. The D4 event vocabulary is
  // frozen at nine names (log/event_log.hpp) and consumed by the D7 checker
  // tools/comms_check/check_v2x_log.py, so inventing a tenth "internal_error"
  // event is not an option; decode_reject is the nearest stage reject and
  // already means "this datagram produced no R2". The log still tells the two
  // cases apart without a new event: a decode-stage throw has NO preceding
  // decode_ok line, while a validator/deduper/builder/sink throw does.
  try {
    // Stamp receipt time BEFORE any work, so rxTime measures arrival rather
    // than processing latency.
    const std::int64_t rx_time_ms = now_ms_();
    log_.rxDatagram(bytes.size());

    // --- Stage 1: decode (HLD D3.1). bytes.data() is null for an empty
    // datagram; that is a legitimate malformed input the codec must reject
    // (corpus: tests/fixtures/malformed/empty.uper), not a precondition here.
    const std::variant<v2x::codec::CpmContent, v2x::codec::DecodeError> outcome =
        codec_.decode(bytes.data(), bytes.size());
    if (const auto* error = std::get_if<v2x::codec::DecodeError>(&outcome)) {
      log_.decodeReject(error->reason);
      return;
    }
    const v2x::codec::CpmContent& content = std::get<v2x::codec::CpmContent>(outcome);
    log_.decodeOk(content);

    // --- Stage 2: profile validation (HLD D3.2). Reject by typed reason; the
    // stable snake_case token is the count-by-reason key.
    if (const std::optional<ValidateReject> reason = validator_.validate(content)) {
      log_.validateReject(toString(*reason));
      return;
    }

    // --- Stage 3: duplicate drop inside the sliding window (HLD D3.3).
    if (deduper_.shouldDrop(content)) {
      log_.dedupeDrop();
      return;
    }

    // --- Stage 4: build the R2 message (a) and hand it to the sink (b)
    // (HLD D3.4). r2_forwarded is emitted AFTER the sink returns, so the event
    // means "handed to the egress", never "attempted".
    const v2x::contracts::R2Message r2 = builder_.build(content, rx_time_ms);
    sink_(r2);
    log_.r2Forwarded(r2);
  } catch (const std::exception& e) {
    reportUnexpected(e.what());
  } catch (...) {
    reportUnexpected("non-std exception");
  }
}

void RxPipeline::reportUnexpected(const char* what) noexcept {
  try {
    log_.decodeReject(std::string(kUnexpectedPrefix) + what);
  } catch (...) {
    // Last resort: the evidence sink itself failed (or allocation did). There
    // is nowhere left to report to, and nothing may escape to the Rx thread
    // (R9 zero-crash) — so swallow deliberately and let the next datagram try.
  }
}

}  // namespace v2x::pipeline
