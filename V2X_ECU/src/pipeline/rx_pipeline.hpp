#ifndef V2X_ECU_PIPELINE_RX_PIPELINE_HPP
#define V2X_ECU_PIPELINE_RX_PIPELINE_HPP

// V2X_ECU/src/pipeline/rx_pipeline.hpp — the R9 Rx pipeline composition root
// (subtask 9.1.4.4; HLD decision D3): one datagram in, at most one R2 message
// out, every stage outcome on the R18 evidence stream.
//
//   bytes → ICpmCodec::decode → Validator → Deduper → R2Builder → R2Sink
//
// The four stages run SYNCHRONOUSLY on the Rx thread (HLD D3: 10 Hz load, no
// queueing in M1), so onDatagram() is the whole pipeline — there is no internal
// thread, queue, or timer here.
//
// TRANSPORT-BLIND (HLD D1): this class never names a socket, an address or a
// port. Both edges are injected — datagram bytes arrive through onDatagram()
// (the R7 adapter's RxCallback calls it), and the built R2 message leaves
// through the R2Sink callback (main wires it to forward::AdaForwarder). Neither
// this header nor rx_pipeline.cpp may include a transport header
// (tools/check_transport_imports.py enforces it), and neither reads env —
// tunables (the dedupe window) are configured on the collaborators the caller
// constructs.
//
// COUNTING lives in EventLog, not here: the frozen "counters" object is bumped
// by the emitting method (see log/event_log.hpp), so this class must never keep
// a parallel tally — one counter per stage, one owner.
//
// LIFETIME: every collaborator is taken by reference and owned by the CALLER
// (main's composition root, 8.1.5.1) — each must outlive the RxPipeline. The
// sink is stored by value (std::function), so a lambda capturing caller state
// must keep that state alive just as long.

#include <cstdint>
#include <functional>
#include <vector>

#include "codec/cpm_codec.hpp"
#include "contracts/r2_message.hpp"
#include "log/event_log.hpp"
#include "pipeline/deduper.hpp"
#include "pipeline/r2_builder.hpp"
#include "pipeline/validator.hpp"

namespace v2x::pipeline {

// Stage-4b egress: where a successfully built R2 message goes. Deliberately a
// std::function of the contract type only — that is what keeps the pipeline
// transport-blind (HLD D1).
using R2Sink = std::function<void(const v2x::contracts::R2Message&)>;

class RxPipeline {
 public:
  // `codec`, `validator`, `deduper`, `builder`, `log`: caller-owned
  // collaborators, each must outlive this object (see header comment).
  // `sink`: stage-4b egress, invoked once per forwarded message.
  // `now_ms`: rx-time source, ms since the Unix epoch — the value stamped into
  //           R2Message::rxTime. An empty function selects the built-in
  //           system_clock wrapper. system_clock, NOT steady_clock: rxTime is
  //           a wall-clock epoch timestamp per the R2 schema (the Deduper's
  //           separate clock is the monotonic one).
  RxPipeline(v2x::codec::ICpmCodec& codec, Validator& validator, Deduper& deduper,
             R2Builder& builder, v2x::log::EventLog& log, R2Sink sink,
             std::function<std::int64_t()> now_ms = {});

  // Holds references — copying would silently alias the stateful stages.
  RxPipeline(const RxPipeline&) = delete;
  RxPipeline& operator=(const RxPipeline&) = delete;

  // The single entry point, called from the Rx thread once per received
  // datagram. Runs all four stages and returns; a rejected/dropped datagram
  // simply produces no sink call.
  //
  // noexcept is a HARD GUARANTEE, not a hint (R9 zero-crash): the whole body
  // is wrapped in catch(...) so nothing — not a codec exception, not a
  // throwing sink, not a bad_alloc — escapes into the Rx thread. Attribution
  // of an unexpected throw is documented in rx_pipeline.cpp.
  void onDatagram(const std::vector<std::uint8_t>& bytes) noexcept;

 private:
  // Emits the single reject event that stands for "an unexpected exception
  // ended this datagram". Never throws itself (see rx_pipeline.cpp).
  void reportUnexpected(const char* what) noexcept;

  v2x::codec::ICpmCodec& codec_;
  Validator& validator_;
  Deduper& deduper_;
  R2Builder& builder_;
  v2x::log::EventLog& log_;
  R2Sink sink_;
  std::function<std::int64_t()> now_ms_;
};

}  // namespace v2x::pipeline

#endif  // V2X_ECU_PIPELINE_RX_PIPELINE_HPP
