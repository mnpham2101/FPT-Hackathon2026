#ifndef V2X_ECU_PIPELINE_VALIDATOR_HPP
#define V2X_ECU_PIPELINE_VALIDATOR_HPP

// V2X_ECU/src/pipeline/validator.hpp — stage 2 of the R9 Rx pipeline (Phase 1
// HLD decision D3): mandatory-field presence + profile-range validation of a decoded
// CpmContent per contracts/r1-cpm-profile.md, incl. F9.
//
// Mandatory-field presence is enforced structurally: every schema-required
// field is a non-optional member of the CpmContent structs, and the codec
// seam's from_json uses j.at(...), which throws on a missing required key —
// so any CpmContent reaching this stage is complete by construction. What
// remains for this class is the profile-range half.
//
// Ranges are the wire-native bounds of
// V2X_ECU/contracts/r1-cpm-content.schema.json (byte-synced from the master
// contract), plus the F9 profile rule |measurementDeltaTime| ≤ 2047 — the wire
// legally carries −2048 but the profile bans it, so decode alone won't reject
// it. The bounds live as named constants in validator.cpp, each commented
// with its schema property path: they are CONTRACT BOUNDS, not tunables, so
// governing principle 5 (no hardcoded tunables — externalize configuration)
// deliberately does not apply; changing one means re-freezing the R1 contract,
// never reconfiguring a deployment.
//
// The class is stateless and does no logging and no counting — violations are
// returned as a typed reason so the pipeline can count them via the R18 event
// log (HLD D4). No transport includes here (tools/check_transport_imports.py
// applies), and no env reads.

#include <optional>

#include "codec/cpm_codec.hpp"

namespace v2x::pipeline {

// One enumerator per schema property family, so counting-by-reason stays
// meaningful. Families whose C++ type already spans the schema range exactly
// (objectId 0..65535 in uint16_t, classification 0..255 in uint8_t) get no
// enumerator: they cannot be violated post-decode. stationId keeps one — its
// uint32_t is likewise type-tight, but the check documents the schema bound
// and survives any future widening of the struct field.
enum class ValidateReject {
  StationIdRange,            // stationId outside 0..4294967295
  ReferenceTimeRange,        // referenceTime outside 0..4398046511103
  LatitudeRange,             // referencePosition.latitude outside -900000000..900000001
  LongitudeRange,            // referencePosition.longitude outside -1800000000..1800000001
  OrientationRange,          // orientationAngle outside 0..3601
  MdtF9Range,                // |object.measurementDeltaTime| > 2047 (F9; wire -2048 banned)
  PositionCoordinateRange,   // object.position.x or .y outside -131072..131071
  CoordinateConfidenceRange, // object.position.xConfidence or .yConfidence outside 1..4096
  VelocityRange,             // object.velocity.x or .y outside -16383..16383
  ClassConfidenceRange,      // object.classConfidence outside 1..101
};

// Stable snake_case tokens — these are the count-by-reason keys the pipeline
// feeds to the R18 event log, not display text.
inline const char* toString(ValidateReject reason) {
  switch (reason) {
    case ValidateReject::StationIdRange:            return "station_id_range";
    case ValidateReject::ReferenceTimeRange:        return "reference_time_range";
    case ValidateReject::LatitudeRange:             return "latitude_range";
    case ValidateReject::LongitudeRange:            return "longitude_range";
    case ValidateReject::OrientationRange:          return "orientation_range";
    case ValidateReject::MdtF9Range:                return "mdt_f9_range";
    case ValidateReject::PositionCoordinateRange:   return "position_coordinate_range";
    case ValidateReject::CoordinateConfidenceRange: return "coordinate_confidence_range";
    case ValidateReject::VelocityRange:             return "velocity_range";
    case ValidateReject::ClassConfidenceRange:      return "class_confidence_range";
  }
  return "unknown";
}

class Validator {
 public:
  // Returns std::nullopt when every field is within its profile range;
  // otherwise the FIRST violation found, checking in schema document order:
  // stationId → referenceTime → referencePosition.latitude → .longitude →
  // orientationAngle → object.measurementDeltaTime (F9) → position.x → .y →
  // .xConfidence → .yConfidence → velocity.x → .y → classConfidence.
  std::optional<ValidateReject> validate(const v2x::codec::CpmContent& content) const;
};

}  // namespace v2x::pipeline

#endif  // V2X_ECU_PIPELINE_VALIDATOR_HPP
