// V2X_ECU/src/pipeline/r2_builder.cpp — stage 4a of the R9 Rx pipeline.
// Derivation rules and citations: r2_builder.hpp header comment +
// contracts/r1-cpm-profile.md §3 (wire units), §5 (F1/F6/F7).

#include "pipeline/r2_builder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace v2x::pipeline {

namespace {

// --- Contract constants (r1-cpm-profile.md §3 wire units, §5 conventions).
// These are frozen CONTRACT scale factors, not tunables — changing one means
// re-freezing the R1 contract, never reconfiguring a deployment (governing
// principle 5 deliberately does not apply).

// Latitude/Longitude wire unit: 10⁻⁷ degree (profile §3).
constexpr double kTenthMicroDegToDeg = 1e-7;
// Wgs84AngleValue wire unit: 0,1 degree (profile §3, orientationAngle).
constexpr double kDeciDegToDeg = 0.1;
// CartesianCoordinateLarge / VelocityComponentValue / CoordinateConfidence
// wire unit: 0,01 m or 0,01 m/s (profile §3; F6 for the confidence).
constexpr double kCentiToSi = 0.01;
// ConfidenceLevel: 1..100 → fraction via /100 clamped to [0,1]; 101 =
// unavailable → null (F6).
constexpr double kConfidenceLevelScale = 100.0;
constexpr std::uint8_t kConfidenceLevelUnavailable = 101;
// R2 envelope: schemaVersion 1 (profile header: "Profile version 1, matches
// R2 schemaVersion: 1") and the schema's const type discriminator.
constexpr int kSchemaVersion = 1;
constexpr const char* kMessageType = "v2x_object";
// R2 object.classification string domain in M1 (schema: "object class; M1:
// vehicle"; sample: "vehicle"). The profile only ever carries
// TrafficParticipantType passengerCar(5), so every M1 CPM object maps here.
constexpr const char* kClassificationVehicle = "vehicle";

// --- F1 planar small-delta approximation constants.
// Mean Earth radius, metres. Over consecutive-CPM deltas (≤ metres apart at
// 10 Hz, F8) the sub-0.5 % radius/ellipsoid error is far below the position
// wire resolution, so the spherical mean radius suffices.
constexpr double kEarthRadiusM = 6371000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kMsPerSecond = 1000.0;

}  // namespace

v2x::contracts::R2Message R2Builder::build(const v2x::codec::CpmContent& content,
                                           std::int64_t rx_time_ms) {
  v2x::contracts::R2Message msg;
  msg.schemaVersion = kSchemaVersion;
  msg.type = kMessageType;
  msg.stationId = content.stationId;
  msg.rxTime = rx_time_ms;  // receipt time from the pipeline, ms epoch

  // --- Sender (B) reference state: direct unit conversions.
  const double lat_deg = content.referencePosition.latitude * kTenthMicroDegToDeg;
  const double lon_deg = content.referencePosition.longitude * kTenthMicroDegToDeg;
  msg.sender.lat = lat_deg;
  msg.sender.lon = lon_deg;
  msg.sender.heading = content.orientationAngle * kDeciDegToDeg;

  // --- F1: sender.speed from consecutive referencePosition/referenceTime
  // deltas per stationId; nullopt until the 2nd message from that station.
  //
  // Planar small-delta (equirectangular) approximation: consecutive CPMs at
  // 10 Hz (F8) are metres apart, so the WGS84 arc is treated as a flat plane —
  // dy = Δlat[rad]·R, dx = Δlon[rad]·cos(mean lat)·R, both in metres; the
  // curvature error over such deltas is orders of magnitude below the 0,01 m
  // wire resolution. speed = hypot(dx, dy) / Δt[s].
  msg.sender.speed = std::nullopt;
  const auto reference_time_ms = static_cast<std::int64_t>(content.referenceTime);
  const auto it = previous_by_station_.find(content.stationId);
  if (it != previous_by_station_.end()) {
    const StationState& prev = it->second;
    const std::int64_t dt_ms = reference_time_ms - prev.reference_time_ms;
    if (dt_ms > 0) {  // Δt ≤ 0 (replay/reorder) → keep nullopt, still refresh
      const double mean_lat_rad = (lat_deg + prev.lat_deg) / 2.0 * kDegToRad;
      const double dx_m =
          (lon_deg - prev.lon_deg) * kDegToRad * std::cos(mean_lat_rad) * kEarthRadiusM;
      const double dy_m = (lat_deg - prev.lat_deg) * kDegToRad * kEarthRadiusM;
      msg.sender.speed = std::hypot(dx_m, dy_m) / (dt_ms / kMsPerSecond);
    }
  }
  previous_by_station_[content.stationId] =
      StationState{lat_deg, lon_deg, reference_time_ms};

  // --- Perceived object (C).
  const auto& obj = content.object;
  msg.object.objectId = obj.objectId;
  // Relative per the R2 schema ("ms vs the CPM referenceTime", ±2047): the
  // absolute measurement instant is referenceTime + timeOfMeasurement, so the
  // carried value is measurementDeltaTime unchanged.
  msg.object.timeOfMeasurement = obj.measurementDeltaTime;

  const double x_m = obj.position.x * kCentiToSi;
  const double y_m = obj.position.y * kCentiToSi;
  msg.object.position.x = x_m;
  msg.object.position.y = y_m;
  // F6: CoordinateConfidence × 0,01 → metres — an accuracy, not a probability.
  // The R2 schema carries ONE scalar accuracy for the pair, so take the worse
  // (larger) of the two per-axis accuracies — conservative for R13 admission.
  msg.object.position.confidence =
      std::max(obj.position.xConfidence, obj.position.yConfidence) * kCentiToSi;

  // F7: distance = hypot(x, y) in SI metres — derived here, never on the wire.
  // No rounding: the profile defines the value as the plain hypot (the shared
  // sample's 25.03 is that hypot printed to 2 decimals, profile §8).
  msg.object.distance = std::hypot(x_m, y_m);

  // Object speed: scalar m/s from the sender-frame velocity components
  // (0,01 m/s wire units), consistent with F7's hypot composition.
  msg.object.speed =
      std::hypot(obj.velocity.x * kCentiToSi, obj.velocity.y * kCentiToSi);

  msg.object.classification = kClassificationVehicle;

  // F6: ConfidenceLevel 101 → null; 1..100 → /100 clamped to [0,1].
  if (obj.classConfidence == kConfidenceLevelUnavailable) {
    msg.object.confidence = std::nullopt;
  } else {
    msg.object.confidence = std::clamp(
        obj.classConfidence / kConfidenceLevelScale, 0.0, 1.0);
  }

  return msg;
}

}  // namespace v2x::pipeline
