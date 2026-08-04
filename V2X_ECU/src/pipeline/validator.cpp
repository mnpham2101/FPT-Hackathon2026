// V2X_ECU/src/pipeline/validator.cpp — profile-range checks for stage 2 of the
// R9 Rx pipeline (HLD decision D3). Bound provenance: every constant below
// mirrors one property of V2X_ECU/contracts/r1-cpm-content.schema.json (the
// node-local byte-synced copy; normative home contracts/r1-cpm-profile.md §3),
// except the F9 lower bound, which is the profile rule tightening the wire
// range. These are contract bounds, not tunables (governing principle 5 does
// not apply — see validator.hpp).

#include "pipeline/validator.hpp"

#include <cstdint>

namespace v2x::pipeline {

namespace {

// Comparisons run through these widened helpers so a type-tight field (e.g.
// stationId, whose uint32_t spans its schema range exactly) still compiles
// warning-free against its named bounds.
constexpr bool InRangeU(std::uint64_t value, std::uint64_t lo, std::uint64_t hi) {
  return value >= lo && value <= hi;
}
constexpr bool InRangeS(std::int64_t value, std::int64_t lo, std::int64_t hi) {
  return value >= lo && value <= hi;
}

// r1-cpm-content.schema.json: stationId minimum/maximum
constexpr std::uint64_t kStationIdMin = 0;
constexpr std::uint64_t kStationIdMax = 4294967295;

// r1-cpm-content.schema.json: referenceTime minimum/maximum (TimestampIts)
constexpr std::uint64_t kReferenceTimeMin = 0;
constexpr std::uint64_t kReferenceTimeMax = 4398046511103;

// r1-cpm-content.schema.json: referencePosition.latitude minimum/maximum
// (900000001 = unavailable, in-range by contract)
constexpr std::int64_t kLatitudeMin = -900000000;
constexpr std::int64_t kLatitudeMax = 900000001;

// r1-cpm-content.schema.json: referencePosition.longitude minimum/maximum
// (1800000001 = unavailable, in-range by contract)
constexpr std::int64_t kLongitudeMin = -1800000000;
constexpr std::int64_t kLongitudeMax = 1800000001;

// r1-cpm-content.schema.json: orientationAngle minimum/maximum
// (3601 = unavailable, in-range by contract)
constexpr std::uint64_t kOrientationMin = 0;
constexpr std::uint64_t kOrientationMax = 3601;

// F9 (contracts/r1-cpm-profile.md §5): "measurementDeltaTime valid range
// ±2047 ms: [...] R9 rejects violations and counts them (−2048 is
// wire-encodable but profile-invalid)." The schema minimum is the wire's
// −2048; the profile bound below deliberately tightens it to −2047.
constexpr std::int64_t kMdtF9Min = -2047;
// r1-cpm-content.schema.json: object.measurementDeltaTime maximum (wire and
// profile agree on the upper bound)
constexpr std::int64_t kMdtF9Max = 2047;

// r1-cpm-content.schema.json: object.position.x/.y minimum/maximum
// (CartesianCoordinateLarge)
constexpr std::int64_t kPositionCoordMin = -131072;
constexpr std::int64_t kPositionCoordMax = 131071;

// r1-cpm-content.schema.json: object.position.xConfidence/.yConfidence
// minimum/maximum (CoordinateConfidence; 4096 = unavailable, in-range)
constexpr std::uint64_t kCoordConfidenceMin = 1;
constexpr std::uint64_t kCoordConfidenceMax = 4096;

// r1-cpm-content.schema.json: object.velocity.x/.y minimum/maximum
// (VelocityComponentValue)
constexpr std::int64_t kVelocityMin = -16383;
constexpr std::int64_t kVelocityMax = 16383;

// r1-cpm-content.schema.json: object.classConfidence minimum/maximum
// (ConfidenceLevel; 101 = unavailable, in-range)
constexpr std::uint64_t kClassConfidenceMin = 1;
constexpr std::uint64_t kClassConfidenceMax = 101;

}  // namespace

std::optional<ValidateReject> Validator::validate(const v2x::codec::CpmContent& content) const {
  // Check order = schema document order; the first violation wins (see the
  // header contract). Aliases keep the checks readable.
  const auto& pos = content.object.position;
  const auto& vel = content.object.velocity;

  if (!InRangeU(content.stationId, kStationIdMin, kStationIdMax)) {
    return ValidateReject::StationIdRange;
  }
  if (!InRangeU(content.referenceTime, kReferenceTimeMin, kReferenceTimeMax)) {
    return ValidateReject::ReferenceTimeRange;
  }
  if (!InRangeS(content.referencePosition.latitude, kLatitudeMin, kLatitudeMax)) {
    return ValidateReject::LatitudeRange;
  }
  if (!InRangeS(content.referencePosition.longitude, kLongitudeMin, kLongitudeMax)) {
    return ValidateReject::LongitudeRange;
  }
  if (!InRangeU(content.orientationAngle, kOrientationMin, kOrientationMax)) {
    return ValidateReject::OrientationRange;
  }
  if (!InRangeS(content.object.measurementDeltaTime, kMdtF9Min, kMdtF9Max)) {
    return ValidateReject::MdtF9Range;
  }
  if (!InRangeS(pos.x, kPositionCoordMin, kPositionCoordMax) ||
      !InRangeS(pos.y, kPositionCoordMin, kPositionCoordMax)) {
    return ValidateReject::PositionCoordinateRange;
  }
  if (!InRangeU(pos.xConfidence, kCoordConfidenceMin, kCoordConfidenceMax) ||
      !InRangeU(pos.yConfidence, kCoordConfidenceMin, kCoordConfidenceMax)) {
    return ValidateReject::CoordinateConfidenceRange;
  }
  if (!InRangeS(vel.x, kVelocityMin, kVelocityMax) ||
      !InRangeS(vel.y, kVelocityMin, kVelocityMax)) {
    return ValidateReject::VelocityRange;
  }
  if (!InRangeU(content.object.classConfidence, kClassConfidenceMin, kClassConfidenceMax)) {
    return ValidateReject::ClassConfidenceRange;
  }
  return std::nullopt;
}

}  // namespace v2x::pipeline
