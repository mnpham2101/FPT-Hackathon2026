// Tests for the R15 warning builder (subtask 15.4.2.1): every emitted shape
// is checked against the synced ADA_ECU/contracts/r4-ada-ivi.schema.json,
// loaded from disk — SCHEMA-GUIDED STRUCTURAL assertion: the required arrays,
// the type const and the geometry key set are read from the schema file, not
// restated as literals, so a schema edit fails these tests rather than
// silently drifting past them. Full JSON-Schema validation of the wire shape
// is the e2e lane's receiver (--validate); no external validator dependency
// enters the unit build.
//
// Cases: a medium and a high transition, and both null-vehicleC geometries —
// before C is first tracked and after its track is erased (the frozen R4 null
// case, both directions of it). geometry.vehicleB must be numeric in every
// case (D5's b_unknown invariant) and `object` must carry all nine required
// R3 fields of the embedded r3-tracked-object.schema.json.

#include "output/warning_builder.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>
#include <string>

#include "contracts/r4_message.hpp"
#include "contracts/tracked_object.hpp"
#include "cra/i_collision_risk_assessment.hpp"
#include "fusion/scene_composer.hpp"
#include "store/track_store.hpp"

namespace {

using ada::contracts::R4WarningEvent;
using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::contracts::Vec2;
using ada::cra::RiskFinding;
using ada::fusion::compose;
using ada::fusion::SceneGeometry;
using ada::output::buildWarning;
using ada::store::TrackStore;

// Loads one synced schema from the node's contracts/ folder (compile
// definition ADA_CONTRACTS_DIR — the ADA_FIXTURE_DIR pattern).
nlohmann::json loadSchema(const std::string& filename) {
  const std::string path = std::string(ADA_CONTRACTS_DIR) + "/" + filename;
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open()) << "cannot open schema: " << path;
  return nlohmann::json::parse(file);
}

TrackedObject MakeTrack(const std::string& id, Source source, double distance, double y) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = source;
  o.position = {distance, y};
  o.distance = distance;
  o.speed = 12.5;
  o.confidence = 0.9;
  o.state = TrackState::tracked;
  o.timestamps = {1789000000000, 1789000000050, 1789000000060};
  return o;
}

RiskFinding MakeFinding(const std::string& riskState, const TrackedObject& trigger,
                        const std::string& rationale) {
  return RiskFinding{"nlos_obstruction", riskState,
                     std::optional<TrackedObject>(trigger), rationale};
}

// Schema-guided structural assertion of one serialized warning event: every
// key the schema's warningEvent $defs requires is present with the right JSON
// type; type carries the schema's const; ego/vehicleB are objects with
// numeric x/y; vehicleC is object-or-null.
void ExpectWarningShape(const nlohmann::json& j) {
  const nlohmann::json schema = loadSchema("r4-ada-ivi.schema.json");
  const nlohmann::json& warningDef = schema.at("$defs").at("warningEvent");

  for (const auto& key : warningDef.at("required")) {
    EXPECT_TRUE(j.contains(key.get<std::string>()))
        << "missing required key: " << key.get<std::string>();
  }
  EXPECT_TRUE(j.at("schemaVersion").is_number_integer());
  EXPECT_GE(j.at("schemaVersion").get<int>(),
            warningDef.at("properties").at("schemaVersion").at("minimum").get<int>());
  EXPECT_EQ(j.at("type"), warningDef.at("properties").at("type").at("const"));
  EXPECT_TRUE(j.at("warningType").is_string());
  EXPECT_TRUE(j.at("riskState").is_string());

  // `object` — all nine required fields of the embedded R3 schema.
  EXPECT_TRUE(j.at("object").is_object());
  const nlohmann::json r3Schema = loadSchema("r3-tracked-object.schema.json");
  for (const auto& key : r3Schema.at("required")) {
    EXPECT_TRUE(j.at("object").contains(key.get<std::string>()))
        << "object missing R3 field: " << key.get<std::string>();
  }

  // `geometry` — the schema's own required key set, then the per-vehicle types.
  const nlohmann::json& geometryDef = warningDef.at("properties").at("geometry");
  const nlohmann::json& g = j.at("geometry");
  ASSERT_TRUE(g.is_object());
  for (const auto& key : geometryDef.at("required")) {
    EXPECT_TRUE(g.contains(key.get<std::string>()))
        << "geometry missing required key: " << key.get<std::string>();
  }
  for (const char* vehicle : {"ego", "vehicleB"}) {
    ASSERT_TRUE(g.at(vehicle).is_object()) << vehicle << " must be an object, never null";
    EXPECT_TRUE(g.at(vehicle).at("x").is_number());
    EXPECT_TRUE(g.at(vehicle).at("y").is_number());
  }
  ASSERT_TRUE(g.at("vehicleC").is_object() || g.at("vehicleC").is_null());
  if (g.at("vehicleC").is_object()) {
    EXPECT_TRUE(g.at("vehicleC").at("x").is_number());
    EXPECT_TRUE(g.at("vehicleC").at("y").is_number());
  }
}

// --- Composed-C cases: medium and high ---------------------------------------

TEST(WarningBuilder, MediumCaseMapsFieldForFieldAndMatchesSchema) {
  TrackStore store;
  store.upsert(MakeTrack("own:1", Source::own_sensor, 20.0, 0.5));
  const TrackedObject trackedC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 25.0, -0.3);
  const auto geometry = compose(store, trackedC, std::nullopt);
  ASSERT_TRUE(geometry.has_value());

  const RiskFinding finding = MakeFinding("medium", trackedC, "range");
  const R4WarningEvent event = buildWarning(finding, *geometry);

  const nlohmann::json j = event;
  ExpectWarningShape(j);
  EXPECT_EQ(j.at("warningType"), "nlos_obstruction");
  EXPECT_EQ(j.at("riskState"), "medium");
  EXPECT_EQ(j.at("object"), nlohmann::json(trackedC));
  EXPECT_DOUBLE_EQ(j.at("geometry").at("ego").at("x").get<double>(), 0.0);
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleB").at("x").get<double>(), 20.0);
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleB").at("y").get<double>(), 0.5);
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleC").at("x").get<double>(), 45.0);
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleC").at("y").get<double>(), 0.2);
}

TEST(WarningBuilder, HighCaseMatchesSchema) {
  TrackStore store;
  store.upsert(MakeTrack("own:1", Source::own_sensor, 12.0, 0.1));
  const TrackedObject trackedC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 15.0, 0.4);
  const auto geometry = compose(store, trackedC, std::nullopt);
  ASSERT_TRUE(geometry.has_value());

  const R4WarningEvent event = buildWarning(MakeFinding("high", trackedC, "range"), *geometry);

  const nlohmann::json j = event;
  ExpectWarningShape(j);
  EXPECT_EQ(j.at("riskState"), "high");
  ASSERT_TRUE(j.at("geometry").at("vehicleC").is_object());
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleC").at("x").get<double>(), 27.0);
}

// --- Null-vehicleC cases: before first tracked, and after erasure ------------

TEST(WarningBuilder, NullVehicleCBeforeCIsFirstTracked) {
  // B in the store, C not yet tracked: the composer yields vehicleC nullopt
  // and the builder must serialize it as JSON null with vehicleB numeric.
  TrackStore store;
  store.upsert(MakeTrack("own:1", Source::own_sensor, 20.0, 0.5));
  const auto geometry = compose(store, std::nullopt, std::nullopt);
  ASSERT_TRUE(geometry.has_value());

  // The trigger snapshot still exists (a finding always carries one on a
  // committed transition — here C's last relayed snapshot).
  const TrackedObject snapshotC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 25.0, -0.3);
  const R4WarningEvent event = buildWarning(MakeFinding("low", snapshotC, "clear"), *geometry);

  const nlohmann::json j = event;
  ExpectWarningShape(j);
  EXPECT_TRUE(j.at("geometry").at("vehicleC").is_null());
  ASSERT_TRUE(j.at("geometry").at("vehicleB").is_object());
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleB").at("x").get<double>(), 20.0);
}

TEST(WarningBuilder, NullVehicleCAfterTrackErasure) {
  // Everything erased from the store; the remembered lastKnownB composes the
  // clearing event's geometry — vehicleB numeric, vehicleC null (D5).
  TrackStore store;
  const auto geometry = compose(store, std::nullopt, Vec2{20.0, 0.5});
  ASSERT_TRUE(geometry.has_value());

  const TrackedObject lastSnapshot = MakeTrack("v2x:1001:7", Source::v2x_relayed, 25.0, -0.3);
  const R4WarningEvent event =
      buildWarning(MakeFinding("low", lastSnapshot, "no_tracked_c"), *geometry);

  const nlohmann::json j = event;
  ExpectWarningShape(j);
  EXPECT_TRUE(j.at("geometry").at("vehicleC").is_null());
  ASSERT_TRUE(j.at("geometry").at("vehicleB").is_object());
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleB").at("x").get<double>(), 20.0);
  EXPECT_DOUBLE_EQ(j.at("geometry").at("vehicleB").at("y").get<double>(), 0.5);
  // The snapshot rides through even though the live track is gone.
  EXPECT_EQ(j.at("object").at("id"), "v2x:1001:7");
}

}  // namespace
