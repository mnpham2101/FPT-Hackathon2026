// Additive-version test for the R4 ADA-to-IVI binding (subtask 4.0.4.4, HLD
// D4): a future-versioned warning event — higher schemaVersion, unknown
// warningType, one unknown extra field — must parse through the unmodified
// 4.0.4.3 binding. Guards ADA-side R4 consumption in R18 tooling. Driven by
// the shared frozen fixture synced to ADA_ECU/tests/fixtures/samples/.

#include "contracts/r4_message.hpp"

#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using ada::contracts::R4WarningEvent;

nlohmann::json LoadUnknownWarningFixture() {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/r4-unknown-warning.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

TEST(R4AdditiveVersion, ParsesWithoutError) {
  const nlohmann::json original = LoadUnknownWarningFixture();
  EXPECT_NO_THROW(original.get<R4WarningEvent>());
}

TEST(R4AdditiveVersion, PreservesUnknownWarningTypeAndFutureVersion) {
  const auto warning = LoadUnknownWarningFixture().get<R4WarningEvent>();

  // Unknown registry key carried through as an opaque string, not rejected.
  EXPECT_EQ(warning.warningType, "slippery_road");
  // Future schema version carried through untouched.
  EXPECT_EQ(warning.schemaVersion, 2);
}

TEST(R4AdditiveVersion, IgnoresUnknownFieldAndRoundTripsKnownFields) {
  const nlohmann::json original = LoadUnknownWarningFixture();
  ASSERT_TRUE(original.contains("hazardDetail"));  // fixture carries the unknown field

  const auto warning = original.get<R4WarningEvent>();

  // Re-emitted JSON drops the unknown field: the binding never stores it.
  const nlohmann::json reemitted = warning;
  EXPECT_FALSE(reemitted.contains("hazardDetail"));

  // All known fields survive the round-trip: struct equality after re-parse.
  const auto reparsed = nlohmann::json::parse(reemitted.dump()).get<R4WarningEvent>();
  EXPECT_TRUE(reparsed == warning);
}

}  // namespace
