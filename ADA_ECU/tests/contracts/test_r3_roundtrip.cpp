// Round-trip test for the R3 TrackedObject binding (subtask 3.0.4.2), driven by
// the shared frozen sample synced to ADA_ECU/tests/fixtures/samples/.

#include "contracts/tracked_object.hpp"

#include <cstdint>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;

nlohmann::json LoadFixture() {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/r3-tracked-object.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

TEST(R3TrackedObjectRoundTrip, StructAndWireEquality) {
  const nlohmann::json original = LoadFixture();
  const auto obj = original.get<TrackedObject>();

  const nlohmann::json reemitted = obj;
  const auto reparsed = nlohmann::json::parse(reemitted.dump()).get<TrackedObject>();

  EXPECT_TRUE(reparsed == obj);
  // Semantic wire equality: the re-emitted JSON matches the original document.
  EXPECT_EQ(reemitted, original);
}

TEST(R3TrackedObjectRoundTrip, FieldSpotChecks) {
  const auto obj = LoadFixture().get<TrackedObject>();

  EXPECT_EQ(obj.source, Source::v2x_relayed);  // the ghost-C shape
  EXPECT_EQ(obj.object_class, "vehicle");
  EXPECT_EQ(obj.state, TrackState::tracked);
  EXPECT_EQ(obj.timestamps.measured, static_cast<std::int64_t>(1789000000073));
}

}  // namespace
