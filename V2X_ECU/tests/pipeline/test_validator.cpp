// Profile-validator test (subtask 9.1.4.1) — stage 2 of the R9 Rx pipeline
// (HLD decision D3).
//
// Two halves:
//   1. The frozen golden corpus (tests/fixtures/golden/*.json, byte-synced
//      from contracts/golden-vectors/) is valid-by-construction — every case
//      must validate to std::nullopt.
//   2. One test per reject reason: start from the nominal golden content,
//      mutate exactly ONE field out of its schema/profile bound, and assert
//      the exact ValidateReject enumerator. Includes the F9 special case
//      mdt = −2048: wire-encodable (schema minimum −2048) but profile-banned,
//      so decode alone would let it through and only this stage rejects it.

#include "pipeline/validator.hpp"

#include <fstream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using v2x::codec::CpmContent;
using v2x::pipeline::ValidateReject;
using v2x::pipeline::Validator;
using v2x::pipeline::toString;

// --- Half 1: every frozen golden vector passes ------------------------------

class GoldenContentValidTest : public ::testing::TestWithParam<const char*> {};

TEST_P(GoldenContentValidTest, ValidatesToNullopt) {
  const std::string path =
      std::string(V2X_FIXTURE_DIR) + "/golden/" + GetParam() + ".json";
  std::ifstream in(path);
  ASSERT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  const auto content = nlohmann::json::parse(in).get<CpmContent>();

  const Validator validator;
  const auto verdict = validator.validate(content);
  EXPECT_FALSE(verdict.has_value())
      << "frozen-valid vector rejected: " << toString(*verdict);
}

// gtest test-name suffixes must be alphanumeric/underscore, so the hyphenated
// case names are transliterated (pattern: test_cpm_golden_vectors.cpp).
std::string CaseName(const ::testing::TestParamInfo<const char*>& info) {
  std::string name = info.param;
  for (char& ch : name) {
    if (ch == '-') {
      ch = '_';
    }
  }
  return name;
}

INSTANTIATE_TEST_SUITE_P(FrozenCorpus, GoldenContentValidTest,
                         ::testing::Values("nominal", "mdt-max", "mdt-min", "conf-unavailable",
                                           "gate-boundary", "coord-large"),
                         CaseName);

// --- Half 2: one mutation per reject reason ---------------------------------

class ValidatorRejectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string path = std::string(V2X_FIXTURE_DIR) + "/golden/nominal.json";
    std::ifstream in(path);
    ASSERT_TRUE(in.is_open()) << "cannot open fixture: " << path;
    content_ = nlohmann::json::parse(in).get<CpmContent>();
    // Sanity: mutations below start from a passing baseline.
    ASSERT_EQ(validator_.validate(content_), std::nullopt);
  }

  Validator validator_;
  CpmContent content_;
};

TEST_F(ValidatorRejectTest, StationIdBoundsAreTypeTight) {
  // stationId's schema range 0..4294967295 coincides exactly with uint32_t,
  // so an out-of-range value is unconstructible post-decode — the check in
  // the validator documents the contract bound rather than guarding a
  // reachable violation. Both schema extremes must pass.
  content_.stationId = 0;
  EXPECT_EQ(validator_.validate(content_), std::nullopt);
  content_.stationId = 4294967295u;
  EXPECT_EQ(validator_.validate(content_), std::nullopt);
}

TEST_F(ValidatorRejectTest, ReferenceTimeAboveMax) {
  content_.referenceTime = 4398046511104ull;  // schema maximum + 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::ReferenceTimeRange));
}

TEST_F(ValidatorRejectTest, LatitudeAboveMax) {
  content_.referencePosition.latitude = 900000002;  // schema maximum + 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::LatitudeRange));
}

TEST_F(ValidatorRejectTest, LatitudeBelowMin) {
  content_.referencePosition.latitude = -900000001;  // schema minimum - 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::LatitudeRange));
}

TEST_F(ValidatorRejectTest, LongitudeAboveMax) {
  content_.referencePosition.longitude = 1800000002;  // schema maximum + 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::LongitudeRange));
}

TEST_F(ValidatorRejectTest, LongitudeBelowMin) {
  content_.referencePosition.longitude = -1800000001;  // schema minimum - 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::LongitudeRange));
}

TEST_F(ValidatorRejectTest, OrientationAboveMax) {
  content_.orientationAngle = 3602;  // schema maximum + 1 (3601 = unavailable is valid)
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::OrientationRange));
}

TEST_F(ValidatorRejectTest, MdtWireMinusTwoThousandFortyEightIsProfileBanned) {
  // F9: the wire (schema minimum) legally carries -2048, but the profile bans
  // it — this is the one value decode alone cannot reject.
  content_.object.measurementDeltaTime = -2048;
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::MdtF9Range));
}

TEST_F(ValidatorRejectTest, MdtAboveF9Max) {
  content_.object.measurementDeltaTime = 2048;  // F9/wire maximum + 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::MdtF9Range));
}

TEST_F(ValidatorRejectTest, PositionXAboveCartesianCoordinateLargeMax) {
  content_.object.position.x = 131072;  // schema maximum + 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::PositionCoordinateRange));
}

TEST_F(ValidatorRejectTest, PositionYBelowCartesianCoordinateLargeMin) {
  content_.object.position.y = -131073;  // schema minimum - 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::PositionCoordinateRange));
}

TEST_F(ValidatorRejectTest, CoordinateConfidenceZero) {
  content_.object.position.xConfidence = 0;  // schema minimum - 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::CoordinateConfidenceRange));
}

TEST_F(ValidatorRejectTest, CoordinateConfidenceAboveMax) {
  content_.object.position.yConfidence = 4097;  // schema maximum + 1 (4096 = unavailable is valid)
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::CoordinateConfidenceRange));
}

TEST_F(ValidatorRejectTest, VelocityXAboveMax) {
  content_.object.velocity.x = 16384;  // schema maximum + 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::VelocityRange));
}

TEST_F(ValidatorRejectTest, VelocityYBelowMin) {
  content_.object.velocity.y = -16384;  // schema minimum - 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::VelocityRange));
}

TEST_F(ValidatorRejectTest, ClassConfidenceZero) {
  content_.object.classConfidence = 0;  // schema minimum - 1
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::ClassConfidenceRange));
}

TEST_F(ValidatorRejectTest, ClassConfidenceAboveMax) {
  content_.object.classConfidence = 102;  // schema maximum + 1 (101 = unavailable is valid)
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::ClassConfidenceRange));
}

TEST_F(ValidatorRejectTest, FirstViolationInSchemaOrderWins) {
  // Documented contract: checks run in schema document order and the first
  // violation is returned — latitude precedes classConfidence.
  content_.referencePosition.latitude = 900000002;
  content_.object.classConfidence = 102;
  EXPECT_EQ(validator_.validate(content_),
            std::optional<ValidateReject>(ValidateReject::LatitudeRange));
}

// --- toString ---------------------------------------------------------------

TEST(ValidateRejectToString, CoversEveryEnumerator) {
  EXPECT_STREQ(toString(ValidateReject::StationIdRange), "station_id_range");
  EXPECT_STREQ(toString(ValidateReject::ReferenceTimeRange), "reference_time_range");
  EXPECT_STREQ(toString(ValidateReject::LatitudeRange), "latitude_range");
  EXPECT_STREQ(toString(ValidateReject::LongitudeRange), "longitude_range");
  EXPECT_STREQ(toString(ValidateReject::OrientationRange), "orientation_range");
  EXPECT_STREQ(toString(ValidateReject::MdtF9Range), "mdt_f9_range");
  EXPECT_STREQ(toString(ValidateReject::PositionCoordinateRange), "position_coordinate_range");
  EXPECT_STREQ(toString(ValidateReject::CoordinateConfidenceRange),
               "coordinate_confidence_range");
  EXPECT_STREQ(toString(ValidateReject::VelocityRange), "velocity_range");
  EXPECT_STREQ(toString(ValidateReject::ClassConfidenceRange), "class_confidence_range");
}

}  // namespace
