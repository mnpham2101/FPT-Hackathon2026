// R2 parser test (subtask 2.2.3.1): the frozen sample
// tests/fixtures/samples/r2-object.json maps field-by-field to the expected
// TrackedObject through the frozen contracts::R2Message binding, the two
// out-of-band values ride on the result, and every reject reason has a case.

#include "parser/r2_parser.hpp"

#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "contracts/tracked_object.hpp"

namespace {

using ada::contracts::Source;
using ada::contracts::TrackState;
using ada::parser::R2Parser;
using ada::parser::R2RejectReason;

nlohmann::json LoadFixture() {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/r2-object.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

TEST(R2ParserTest, SampleMapsFieldByField) {
  R2Parser parser;
  const auto result = parser.parse(LoadFixture().dump());

  ASSERT_TRUE(result.accepted());
  EXPECT_EQ(result.reason, R2RejectReason::None);
  const auto& obj = *result.object;

  // id convention exact: "v2x:" + stationId + ":" + objectId.
  EXPECT_EQ(obj.id, "v2x:1201:7");
  EXPECT_EQ(obj.object_class, "vehicle");
  EXPECT_EQ(obj.source, Source::v2x_relayed);
  EXPECT_DOUBLE_EQ(obj.position.x, 25.0);
  EXPECT_DOUBLE_EQ(obj.position.y, 1.2);
  EXPECT_DOUBLE_EQ(obj.distance, 25.03);
  EXPECT_DOUBLE_EQ(obj.speed, 15.2);
  EXPECT_DOUBLE_EQ(obj.confidence, 0.95);

  // state is a placeholder — the store is the sole writer (D3).
  EXPECT_EQ(obj.state, TrackState::not_tracked);

  // measured = rxTime + timeOfMeasurement = 1789000000123 + (-50), one clock
  // domain (D10); received = the sample's rxTime, a record value.
  EXPECT_EQ(obj.timestamps.measured, 1789000000073LL);
  EXPECT_EQ(obj.timestamps.received, 1789000000123LL);
  // lastUpdated placeholder — the store stamps its own clock (HLD §10.2).
  EXPECT_EQ(obj.timestamps.lastUpdated, 0LL);

  EXPECT_EQ(parser.acceptedCount(), 1u);
  EXPECT_EQ(parser.rejectedCount(), 0u);
}

TEST(R2ParserTest, OutOfBandValuesRideBesideTheObject) {
  R2Parser parser;
  const auto result = parser.parse(LoadFixture().dump());

  ASSERT_TRUE(result.accepted());
  // object.confidence exactly as received.
  ASSERT_TRUE(result.receivedObjectConfidence.has_value());
  EXPECT_DOUBLE_EQ(*result.receivedObjectConfidence, 0.95);
  // position.confidence (metres, F6) has no R3 home — out-of-band only.
  ASSERT_TRUE(result.positionConfidence.has_value());
  EXPECT_DOUBLE_EQ(*result.positionConfidence, 0.9);
}

TEST(R2ParserTest, NullObjectConfidenceMapsToZeroAndStaysVisible) {
  nlohmann::json doc = LoadFixture();
  doc["object"]["confidence"] = nullptr;  // frozen R2 types it ["number","null"]

  R2Parser parser;
  const auto result = parser.parse(doc.dump());

  ASSERT_TRUE(result.accepted());
  // R3 requires confidence in 0..1: null maps to 0.0, the lowest confidence.
  EXPECT_DOUBLE_EQ(result.object->confidence, 0.0);
  // The received null stays visible out-of-band for the r2_ingest payload.
  EXPECT_FALSE(result.receivedObjectConfidence.has_value());
}

TEST(R2ParserTest, RejectsEmptyInput) {
  R2Parser parser;
  EXPECT_EQ(parser.parse("").reason, R2RejectReason::EmptyInput);
  EXPECT_EQ(parser.parse("  \t\r\n").reason, R2RejectReason::EmptyInput);
  EXPECT_EQ(parser.rejectedCount(R2RejectReason::EmptyInput), 2u);
  EXPECT_EQ(parser.acceptedCount(), 0u);
}

TEST(R2ParserTest, RejectsMalformedJson) {
  R2Parser parser;
  EXPECT_EQ(parser.parse("{\"schemaVersion\": 1,").reason, R2RejectReason::MalformedJson);
  // Parseable JSON whose top level is not an object is equally malformed here.
  EXPECT_EQ(parser.parse("[1, 2, 3]").reason, R2RejectReason::MalformedJson);
  EXPECT_EQ(parser.rejectedCount(R2RejectReason::MalformedJson), 2u);
}

TEST(R2ParserTest, RejectsWrongType) {
  nlohmann::json doc = LoadFixture();
  doc["type"] = "cam";  // schema const is "v2x_object"

  R2Parser parser;
  EXPECT_EQ(parser.parse(doc.dump()).reason, R2RejectReason::WrongType);
  EXPECT_EQ(parser.rejectedCount(R2RejectReason::WrongType), 1u);
}

TEST(R2ParserTest, RejectsMissingField) {
  nlohmann::json doc = LoadFixture();
  doc.erase("object");

  R2Parser parser;
  EXPECT_EQ(parser.parse(doc.dump()).reason, R2RejectReason::MissingField);
  EXPECT_EQ(parser.rejectedCount(R2RejectReason::MissingField), 1u);
}

TEST(R2ParserTest, RejectsWrongFieldType) {
  nlohmann::json doc = LoadFixture();
  doc["object"]["distance"] = "twenty-five metres";

  R2Parser parser;
  EXPECT_EQ(parser.parse(doc.dump()).reason, R2RejectReason::WrongFieldType);
  EXPECT_EQ(parser.rejectedCount(R2RejectReason::WrongFieldType), 1u);
}

TEST(R2ParserTest, RejectResultCarriesNoObjectOrOutOfBandValues) {
  R2Parser parser;
  const auto result = parser.parse("not json");
  EXPECT_FALSE(result.accepted());
  EXPECT_FALSE(result.object.has_value());
  EXPECT_FALSE(result.receivedObjectConfidence.has_value());
  EXPECT_FALSE(result.positionConfidence.has_value());
}

}  // namespace
