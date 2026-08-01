// Round-trip test for the R4 ADA-to-IVI binding (subtask 4.0.4.3), driven by
// the shared frozen samples synced to ADA_ECU/tests/fixtures/samples/.

#include "contracts/r4_message.hpp"

#include <cstdint>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using ada::contracts::R4StateMessage;
using ada::contracts::R4WarningEvent;
using ada::contracts::Source;

nlohmann::json LoadFixture(const std::string& name) {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/" + name;
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

TEST(R4WarningRoundTrip, StructAndWireEquality) {
  const nlohmann::json original = LoadFixture("r4-warning.json");
  const auto warning = original.get<R4WarningEvent>();

  const nlohmann::json reemitted = warning;
  const auto reparsed = nlohmann::json::parse(reemitted.dump()).get<R4WarningEvent>();

  EXPECT_TRUE(reparsed == warning);
  // Semantic wire equality: the re-emitted JSON matches the original document.
  EXPECT_EQ(reemitted, original);
}

TEST(R4WarningRoundTrip, FieldSpotChecks) {
  const auto warning = LoadFixture("r4-warning.json").get<R4WarningEvent>();

  EXPECT_EQ(warning.type, "warning");
  EXPECT_EQ(warning.warningType, "nlos_obstruction");  // the M1 registry key
  EXPECT_EQ(warning.object.source, Source::v2x_relayed);
  EXPECT_EQ(warning.object.id, "trk-c-001");
  EXPECT_TRUE(warning.geometry.vehicleC.has_value());
}

TEST(R4StateRoundTrip, StructAndWireEquality) {
  const nlohmann::json original = LoadFixture("r4-state.json");
  const auto state = original.get<R4StateMessage>();

  const nlohmann::json reemitted = state;
  const auto reparsed = nlohmann::json::parse(reemitted.dump()).get<R4StateMessage>();

  EXPECT_TRUE(reparsed == state);
  // Semantic wire equality: the re-emitted JSON matches the original document.
  EXPECT_EQ(reemitted, original);
}

TEST(R4StateRoundTrip, FieldSpotChecks) {
  const auto state = LoadFixture("r4-state.json").get<R4StateMessage>();

  EXPECT_EQ(state.type, "state");
  EXPECT_EQ(state.seq, static_cast<std::int64_t>(42));
  EXPECT_TRUE(state.vehicles.vehicleC.has_value());
}

}  // namespace
