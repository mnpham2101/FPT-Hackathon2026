// Tests for the R9 stage-4a R2 builder (subtask 9.1.4.3, Phase 1 HLD §3 D3):
// CpmContent (wire-native integers) → v2x::contracts::R2Message (SI), with the
// F1/F6/F7 derivations cross-checked against the frozen fixtures — the nominal
// golden vector (contracts/golden-vectors, synced to tests/fixtures/golden/)
// as input, the shared R2 sample (tests/fixtures/samples/r2-object.json) as
// the expected output.

#include "pipeline/r2_builder.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "codec/cpm_codec.hpp"
#include "contracts/r2_message.hpp"

namespace {

using v2x::codec::CpmContent;
using v2x::contracts::R2Message;
using v2x::pipeline::R2Builder;

nlohmann::json LoadFixture(const std::string& relative) {
  const std::string path = std::string(V2X_FIXTURE_DIR) + "/" + relative;
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

CpmContent LoadGolden(const std::string& name) {
  return LoadFixture("golden/" + name).get<CpmContent>();
}

// (a) Nominal golden content → every derivable field equals the shared frozen
// sample. rx_time is passed as the sample's rxTime so the stamp matches too.
// Fields that legitimately differ from the sample: sender.speed (F1 — null on
// the first message from a station; the sample shows the steady state) and
// object.distance (the builder emits the exact hypot; the sample carries it
// printed to 2 decimals, profile §8 erratum note).
TEST(R2Builder, NominalMatchesFrozenSample) {
  const nlohmann::json sample = LoadFixture("samples/r2-object.json");
  R2Builder builder;

  const R2Message msg =
      builder.build(LoadGolden("nominal.json"), sample.at("rxTime").get<std::int64_t>());

  EXPECT_EQ(msg.schemaVersion, sample.at("schemaVersion").get<int>());
  EXPECT_EQ(msg.type, sample.at("type").get<std::string>());
  EXPECT_EQ(msg.stationId, sample.at("stationId").get<std::uint32_t>());
  EXPECT_EQ(msg.rxTime, sample.at("rxTime").get<std::int64_t>());

  const auto& sender = sample.at("sender");
  EXPECT_NEAR(msg.sender.lat, sender.at("lat").get<double>(), 1e-9);
  EXPECT_NEAR(msg.sender.lon, sender.at("lon").get<double>(), 1e-9);
  EXPECT_NEAR(msg.sender.heading, sender.at("heading").get<double>(), 1e-9);
  // F1: first message from station 1201 → no previous reference sample yet.
  EXPECT_FALSE(msg.sender.speed.has_value());

  const auto& object = sample.at("object");
  EXPECT_EQ(msg.object.objectId, object.at("objectId").get<int>());
  EXPECT_EQ(msg.object.timeOfMeasurement, object.at("timeOfMeasurement").get<int>());
  EXPECT_NEAR(msg.object.position.x, object.at("position").at("x").get<double>(), 1e-9);
  EXPECT_NEAR(msg.object.position.y, object.at("position").at("y").get<double>(), 1e-9);
  // F6: CoordinateConfidence 90 × 0.01 → 0.9 m.
  EXPECT_NEAR(msg.object.position.confidence,
              object.at("position").at("confidence").get<double>(), 1e-9);
  EXPECT_NEAR(msg.object.speed, object.at("speed").get<double>(), 1e-9);
  EXPECT_EQ(msg.object.classification, object.at("classification").get<std::string>());
  ASSERT_TRUE(msg.object.confidence.has_value());
  EXPECT_NEAR(*msg.object.confidence, object.at("confidence").get<double>(), 1e-9);

  // F7: the exact derivation — hypot of the converted 0.01 m wire units.
  EXPECT_NEAR(msg.object.distance, std::hypot(25.0, 1.2), 1e-9);
  // The sample's 25.03 is that hypot (25.028783...) printed to 2 decimals
  // (profile §8) — agree within half a hundredth.
  EXPECT_NEAR(msg.object.distance, object.at("distance").get<double>(), 5e-3);
}

// (b) F6: ConfidenceLevel 101 = unavailable → null object confidence.
TEST(R2Builder, ClassConfidenceUnavailableMapsToNull) {
  R2Builder builder;
  const R2Message msg = builder.build(LoadGolden("conf-unavailable.json"), 0);
  EXPECT_FALSE(msg.object.confidence.has_value());
}

// (c) F1: null on the 1st message, derived on the 2nd from the position/time
// deltas, null again on a Δt == 0 guard hit.
TEST(R2Builder, SenderSpeedDerivedAcrossConsecutiveMessages) {
  R2Builder builder;
  const CpmContent first = LoadGolden("nominal.json");

  CpmContent second = first;
  second.referencePosition.latitude += 1000;    // +0.0001 deg
  second.referencePosition.longitude += 2000;   // +0.0002 deg
  second.referenceTime += 500;                  // Δt = 500 ms

  const R2Message msg1 = builder.build(first, 0);
  EXPECT_FALSE(msg1.sender.speed.has_value());

  const R2Message msg2 = builder.build(second, 0);
  ASSERT_TRUE(msg2.sender.speed.has_value());

  // Hand-computed expectation via the documented planar small-delta
  // approximation, written out independently of the builder's code:
  // dy = Δlat[rad]·R, dx = Δlon[rad]·cos(mean lat)·R, speed = hypot / Δt[s].
  const double kDegToRad = 3.14159265358979323846 / 180.0;
  const double kEarthRadiusM = 6371000.0;
  const double lat1 = 210285110 * 1e-7, lat2 = 210286110 * 1e-7;
  const double lon1 = 1058048170 * 1e-7, lon2 = 1058050170 * 1e-7;
  const double mean_lat_rad = (lat1 + lat2) / 2.0 * kDegToRad;
  const double dx = (lon2 - lon1) * kDegToRad * std::cos(mean_lat_rad) * kEarthRadiusM;
  const double dy = (lat2 - lat1) * kDegToRad * kEarthRadiusM;
  const double expected = std::hypot(dx, dy) / 0.5;
  EXPECT_NEAR(*msg2.sender.speed, expected, 1e-9);
  // Coarse independent anchor (~23.55 m over 0.5 s at lat 21.03°).
  EXPECT_NEAR(*msg2.sender.speed, 47.10, 0.05);

  // Guard: a repeat with the SAME referenceTime (Δt == 0) must not divide —
  // speed goes back to null.
  const R2Message msg3 = builder.build(second, 0);
  EXPECT_FALSE(msg3.sender.speed.has_value());
}

// (d) F1 history is keyed per stationId — a new station never inherits
// another station's previous reference sample.
TEST(R2Builder, SenderSpeedHistoryIsPerStation) {
  R2Builder builder;
  const CpmContent first = LoadGolden("nominal.json");

  CpmContent second = first;
  second.referencePosition.latitude += 1000;
  second.referenceTime += 500;

  ASSERT_FALSE(builder.build(first, 0).sender.speed.has_value());
  ASSERT_TRUE(builder.build(second, 0).sender.speed.has_value());

  CpmContent other_station = second;
  other_station.stationId = 4202;
  EXPECT_FALSE(builder.build(other_station, 0).sender.speed.has_value());
}

}  // namespace
