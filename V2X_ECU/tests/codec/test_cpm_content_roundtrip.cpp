// Round-trip test for the R1 codec-seam struct CpmContent (subtask 1.0.2.2).
//
// Scope: CpmContent ⇄ JSON ⇄ CpmContent only. There is no ICpmCodec
// implementation yet (subtask 1.0.2.3) and no golden-vector fixture yet
// (subtask 1.0.2.4), so the nominal content is built in code from the frozen
// `nominal` golden vector in contracts/r1-cpm-profile.md §4.

#include "codec/cpm_codec.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using v2x::codec::CpmContent;

// The frozen `nominal` golden vector, as a struct.
CpmContent NominalContent() {
  CpmContent c;
  c.stationId = 1201;
  c.referenceTime = 716084805123;
  c.referencePosition.latitude = 210285110;
  c.referencePosition.longitude = 1058048170;
  c.orientationAngle = 900;
  c.object.objectId = 7;
  c.object.measurementDeltaTime = -50;
  c.object.position.x = 2500;
  c.object.position.y = 120;
  c.object.position.xConfidence = 90;
  c.object.position.yConfidence = 90;
  c.object.velocity.x = 1520;
  c.object.velocity.y = 0;
  c.object.classification = 5;
  c.object.classConfidence = 95;
  return c;
}

// The same frozen `nominal` golden vector, as the wire document.
nlohmann::json NominalDocument() {
  return nlohmann::json::parse(R"({
    "stationId": 1201,
    "referenceTime": 716084805123,
    "referencePosition": { "latitude": 210285110, "longitude": 1058048170 },
    "orientationAngle": 900,
    "object": {
      "objectId": 7,
      "measurementDeltaTime": -50,
      "position": { "x": 2500, "y": 120, "xConfidence": 90, "yConfidence": 90 },
      "velocity": { "x": 1520, "y": 0 },
      "classification": 5,
      "classConfidence": 95
    }
  })");
}

TEST(CpmContentRoundTrip, StructToJsonToStructEquality) {
  const CpmContent content = NominalContent();

  const nlohmann::json j = content;
  const auto reparsed = j.get<CpmContent>();

  EXPECT_TRUE(reparsed == content);
}

TEST(CpmContentRoundTrip, WireShapeMatchesSchemaKeys) {
  // Emitted key names, nesting and wire-native integer units must match
  // contracts/r1-cpm-content.schema.json 1:1 — that is what makes the golden
  // vectors interchangeable across the codec seam.
  const nlohmann::json emitted = NominalContent();

  EXPECT_EQ(emitted, NominalDocument());
}

TEST(CpmContentRoundTrip, RejectsMissingRequiredKey) {
  // from_json uses j.at(...), so a missing schema-required key throws rather
  // than silently defaulting.
  nlohmann::json doc = NominalDocument();
  doc.erase("orientationAngle");

  EXPECT_THROW(doc.get<CpmContent>(), nlohmann::json::out_of_range);
}

}  // namespace
