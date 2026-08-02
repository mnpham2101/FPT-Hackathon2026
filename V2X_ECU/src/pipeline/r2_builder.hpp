#ifndef V2X_ECU_PIPELINE_R2_BUILDER_HPP
#define V2X_ECU_PIPELINE_R2_BUILDER_HPP

// V2X_ECU/src/pipeline/r2_builder.hpp — stage 4a of the R9 Rx pipeline (Phase 1
// HLD §3 D3): maps a decoded CpmContent (wire-native integers, codec seam) to
// the Phase 0 v2x::contracts::R2Message (SI units), owning EVERY derivation the
// codec seam excludes per contracts/r1-cpm-profile.md:
//
//   F7 — object.distance = hypot(position.x, position.y) in metres, derived
//        here from the 0,01 m wire units; never transmitted in the CPM.
//   F6 — confidence conversions: object.confidence = ConfidenceLevel / 100
//        clamped to [0,1], 101 → std::nullopt; position.confidence =
//        CoordinateConfidence × 0,01 metres (an accuracy, not a probability).
//   F1 — sender.speed derived from consecutive referencePosition/referenceTime
//        deltas per stationId; std::nullopt until the 2nd message from that
//        station (and on a non-positive referenceTime delta). The per-station
//        history lives in this class — the builder is the pipeline's only
//        stateful SI-side stage.
//
// Direct unit conversions (profile §3 wire units → R2 schema SI):
// lat/lon 10⁻⁷ ° → °, orientationAngle 0,1 ° → ° (heading), position/velocity
// 0,01 → m and m/s. rxTime is stamped from the value the pipeline passes in
// (datagram receipt time, ms epoch — the R2 schema's rxTime unit); it is NOT
// read from a clock here, so the builder stays deterministic under test.
// object.timeOfMeasurement carries measurementDeltaTime unchanged: the R2
// schema defines it as "ms vs the CPM referenceTime" (range ±2047), i.e. the
// absolute measurement instant is referenceTime + timeOfMeasurement.
//
// Transport-blind pure derivation code: no sockets
// (tools/check_transport_imports.py applies), no env reads, no logging, no
// clock — scale factors are contract constants in r2_builder.cpp, each
// commented with its profile citation (contract bounds, not tunables:
// governing principle 5 deliberately does not apply).

#include <cstdint>
#include <unordered_map>

#include "codec/cpm_codec.hpp"
#include "contracts/r2_message.hpp"

namespace v2x::pipeline {

class R2Builder {
 public:
  // Maps one decoded CPM to one R2 message. `rx_time_ms` is the datagram
  // receipt timestamp supplied by the pipeline, ms epoch — assigned to
  // R2Message::rxTime unchanged (schema: "receive timestamp at the V2X ECU,
  // ms epoch"). Stateful: updates this builder's per-station F1 history.
  v2x::contracts::R2Message build(const v2x::codec::CpmContent& content,
                                  std::int64_t rx_time_ms);

 private:
  // Previous reference sample per station, for the F1 sender-speed
  // derivation. Stored already converted to degrees so build() converts once.
  struct StationState {
    double lat_deg{};
    double lon_deg{};
    std::int64_t reference_time_ms{};  // TimestampIts of the previous CPM
  };

  std::unordered_map<std::uint32_t, StationState> previous_by_station_;
};

}  // namespace v2x::pipeline

#endif  // V2X_ECU_PIPELINE_R2_BUILDER_HPP
