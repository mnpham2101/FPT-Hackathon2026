// Round-trip test for the R2 v2x-object consumer binding (subtask 2.0.3.2),
// driven by the shared frozen sample synced to ADA_ECU/tests/fixtures/samples/.

#include "contracts/r2_message.hpp"

#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using ada::contracts::R2Message;

nlohmann::json LoadFixture() {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/r2-object.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

TEST(R2MessageRoundTrip, StructAndWireEquality) {
  const nlohmann::json original = LoadFixture();
  const auto msg = original.get<R2Message>();

  const nlohmann::json reemitted = msg;
  const auto reparsed = nlohmann::json::parse(reemitted.dump()).get<R2Message>();

  EXPECT_TRUE(reparsed == msg);
  // Semantic wire equality: the re-emitted JSON matches the original document.
  EXPECT_EQ(reemitted, original);
}

TEST(R2MessageRoundTrip, FieldSpotChecks) {
  const auto msg = LoadFixture().get<R2Message>();

  EXPECT_EQ(msg.type, "v2x_object");
  EXPECT_EQ(msg.stationId, 1201u);
  ASSERT_TRUE(msg.sender.speed.has_value());
  EXPECT_DOUBLE_EQ(*msg.sender.speed, 16.7);
  EXPECT_DOUBLE_EQ(msg.object.distance, 25.03);  // F7-derived hypot(position.x, position.y)
  ASSERT_TRUE(msg.object.confidence.has_value());
  EXPECT_DOUBLE_EQ(*msg.object.confidence, 0.95);
}

TEST(R2MessageRoundTrip, NullSenderSpeedRoundTrips) {
  nlohmann::json doc = LoadFixture();
  doc["sender"]["speed"] = nullptr;

  const auto msg = doc.get<R2Message>();
  EXPECT_FALSE(msg.sender.speed.has_value());

  const nlohmann::json reemitted = msg;
  EXPECT_TRUE(reemitted.at("sender").at("speed").is_null());

  const auto reparsed = nlohmann::json::parse(reemitted.dump()).get<R2Message>();
  EXPECT_TRUE(reparsed == msg);
}

}  // namespace
