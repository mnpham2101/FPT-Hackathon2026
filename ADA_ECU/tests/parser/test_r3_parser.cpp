// R3 JSONL parser test (subtask 3.2.3.2): the frozen sample
// tests/fixtures/samples/r3-tracked-object.json maps intact through the
// frozen contracts::TrackedObject binding, the incoming state is discarded
// (D3), a non-own_sensor source is rejected (D6), and malformed lines reject.
//
// The frozen sample carries source "v2x_relayed" (it is the shared R3 sample,
// relayed-shaped), which this parser rejects by design — the maps-intact case
// therefore flips source to "own_sensor" and the source-rejection case uses
// the sample verbatim.

#include "parser/r3_parser.hpp"

#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "contracts/tracked_object.hpp"

namespace {

using ada::contracts::Source;
using ada::contracts::TrackState;
using ada::parser::R3Parser;
using ada::parser::R3RejectReason;

nlohmann::json LoadFixture() {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/r3-tracked-object.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

// The sample as a detector could have emitted it: source own_sensor.
nlohmann::json DetectorShapedFixture() {
  nlohmann::json doc = LoadFixture();
  doc["source"] = "own_sensor";
  return doc;
}

TEST(R3ParserTest, DetectorShapedSampleMapsIntact) {
  R3Parser parser;
  const auto result = parser.parse(DetectorShapedFixture().dump());

  ASSERT_TRUE(result.accepted());
  EXPECT_EQ(result.reason, R3RejectReason::None);
  const auto& obj = *result.object;

  EXPECT_EQ(obj.id, "trk-c-001");
  EXPECT_EQ(obj.object_class, "vehicle");
  EXPECT_EQ(obj.source, Source::own_sensor);
  EXPECT_DOUBLE_EQ(obj.position.x, 55.0);
  EXPECT_DOUBLE_EQ(obj.position.y, 1.7);
  EXPECT_DOUBLE_EQ(obj.distance, 55.03);
  EXPECT_DOUBLE_EQ(obj.speed, 15.2);
  EXPECT_DOUBLE_EQ(obj.confidence, 0.95);
  EXPECT_EQ(obj.timestamps.measured, 1789000000073LL);
  EXPECT_EQ(obj.timestamps.received, 1789000000123LL);
  EXPECT_EQ(obj.timestamps.lastUpdated, 1789000000123LL);  // as parsed; the store overwrites it

  EXPECT_EQ(parser.acceptedCount(), 1u);
  EXPECT_EQ(parser.rejectedCount(), 0u);
}

TEST(R3ParserTest, IncomingStateIsDiscarded) {
  nlohmann::json doc = DetectorShapedFixture();
  ASSERT_EQ(doc.at("state").get<std::string>(), "tracked");  // the sample really claims tracked

  R3Parser parser;
  const auto result = parser.parse(doc.dump());

  ASSERT_TRUE(result.accepted());
  // The store is the sole writer of state (D3): the parser emits the
  // not_tracked placeholder regardless of what the line claimed.
  EXPECT_EQ(result.object->state, TrackState::not_tracked);
}

TEST(R3ParserTest, RejectsRelayedSource) {
  // The sample verbatim: source "v2x_relayed". A detector cannot mint relayed
  // entries — the structural half of the zero-C argument (D6).
  R3Parser parser;
  const auto result = parser.parse(LoadFixture().dump());

  EXPECT_FALSE(result.accepted());
  EXPECT_EQ(result.reason, R3RejectReason::NonOwnSensorSource);
  EXPECT_EQ(parser.rejectedCount(R3RejectReason::NonOwnSensorSource), 1u);
  EXPECT_EQ(parser.acceptedCount(), 0u);
}

TEST(R3ParserTest, RejectsEmptyLine) {
  R3Parser parser;
  EXPECT_EQ(parser.parse("").reason, R3RejectReason::EmptyInput);
  EXPECT_EQ(parser.parse(" \t\r\n").reason, R3RejectReason::EmptyInput);
  EXPECT_EQ(parser.rejectedCount(R3RejectReason::EmptyInput), 2u);
}

TEST(R3ParserTest, RejectsMalformedLine) {
  R3Parser parser;
  EXPECT_EQ(parser.parse("{\"id\": \"trk-").reason, R3RejectReason::MalformedJson);
  EXPECT_EQ(parser.parse("42").reason, R3RejectReason::MalformedJson);  // not an object
  EXPECT_EQ(parser.rejectedCount(R3RejectReason::MalformedJson), 2u);
}

TEST(R3ParserTest, RejectsMissingRequiredField) {
  nlohmann::json doc = DetectorShapedFixture();
  doc.erase("distance");

  R3Parser parser;
  EXPECT_EQ(parser.parse(doc.dump()).reason, R3RejectReason::MissingField);
  EXPECT_EQ(parser.rejectedCount(R3RejectReason::MissingField), 1u);
}

TEST(R3ParserTest, RejectsWrongFieldType) {
  nlohmann::json doc = DetectorShapedFixture();
  doc["speed"] = "fast";

  R3Parser parser;
  EXPECT_EQ(parser.parse(doc.dump()).reason, R3RejectReason::WrongFieldType);
  EXPECT_EQ(parser.rejectedCount(R3RejectReason::WrongFieldType), 1u);
}

TEST(R3ParserTest, RejectsOutOfRangeValues) {
  R3Parser parser;

  nlohmann::json overConfident = DetectorShapedFixture();
  overConfident["confidence"] = 1.5;  // schema bounds confidence to 0..1
  EXPECT_EQ(parser.parse(overConfident.dump()).reason, R3RejectReason::OutOfRangeValue);

  nlohmann::json negativeDistance = DetectorShapedFixture();
  negativeDistance["distance"] = -1.0;  // schema minimum 0
  EXPECT_EQ(parser.parse(negativeDistance.dump()).reason, R3RejectReason::OutOfRangeValue);

  EXPECT_EQ(parser.rejectedCount(R3RejectReason::OutOfRangeValue), 2u);
  EXPECT_EQ(parser.acceptedCount(), 0u);
}

}  // namespace
