// Tests for the R14 assessment database accessor (subtask 14.2.5.3): a record
// round-trips through upsert/get under the composite (trackId, warningType)
// key; the serialized record validates against the committed
// schema/cra-assessment-record.schema.json LOADED FROM DISK, never restated
// here; erase removes every warningType for a track; the committed sample at
// tests/fixtures/cra-assessment-record.json deserializes; and every upsert is
// mirrored to the [EVT] stream as an `assessment` line (injected sink, so no
// test reads real stdout).
//
// Schema validation approach, stated honestly: a minimal in-test validator
// covering exactly the JSON Schema keywords the two committed schemas use —
// `type` (a name or an array of names, including "null"), `required`,
// `properties` (recursive), `enum`, `minimum`, `maximum`, and file-relative
// `$ref`. The `lastSnapshot` $ref is followed to the committed
// contracts/r3-tracked-object.schema.json on disk and validated structurally
// with the same validator (the round trip through the frozen
// contracts::TrackedObject binding covers it a second way, since serialization
// goes through that binding). $schema/$id/title/description are
// annotation-only and skipped. A guard walks each loaded schema and fails the
// test on any constraint keyword outside the covered set, so a schema edit
// cannot silently outgrow this validator.

#include "cra/assessment_db.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "contracts/tracked_object.hpp"
#include "log/event_log.hpp"

namespace {

using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::contracts::Vec2;
using ada::cra::AssessmentDb;
using ada::cra::AssessmentRecord;
using ada::log::EventLog;
namespace events = ada::log::events;

// The schema is NOT under tests/fixtures — reach it relative to the fixture
// dir (ADA_FIXTURE_DIR = <ADA_ECU>/tests/fixtures, so ../../ is <ADA_ECU>).
const std::filesystem::path kSchemaPath =
    std::filesystem::path(ADA_FIXTURE_DIR) / ".." / ".." / "schema" /
    "cra-assessment-record.schema.json";
const std::filesystem::path kSamplePath =
    std::filesystem::path(ADA_FIXTURE_DIR) / "cra-assessment-record.json";

EventLog::Clocks fixedClocks(std::int64_t monoMs, std::int64_t epochMs) {
  EventLog::Clocks clocks;
  clocks.monoMs = [monoMs]() -> std::int64_t { return monoMs; };
  clocks.epochMs = [epochMs]() -> std::int64_t { return epochMs; };
  return clocks;
}

nlohmann::json LoadJsonFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open: " << path.string();
  return nlohmann::json::parse(in);
}

// Asserts the [EVT] prefix and parses the JSON that follows it.
nlohmann::json parseEvtLine(const std::string& line) {
  const std::string prefix = "[EVT] ";
  EXPECT_EQ(line.rfind(prefix, 0), 0u) << "line lacks the [EVT] prefix: " << line;
  return nlohmann::json::parse(line.substr(prefix.size()));
}

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    out.push_back(line);
  }
  return out;
}

// ---------------------------------------------------------------------------
// Minimal JSON Schema validator — the covered-keyword set is documented in the
// file header. Fails the test on any constraint keyword outside that set.
// ---------------------------------------------------------------------------

void AssertOnlyCoveredKeywords(const nlohmann::json& schema, const std::string& where) {
  static const std::set<std::string> covered = {
      "$schema", "$id",      "title",      "description", "type",
      "required", "properties", "enum",    "minimum",     "maximum",
      "$ref"};
  ASSERT_TRUE(schema.is_object()) << "subschema is not an object at " << where;
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    if (covered.find(it.key()) == covered.end()) {
      ADD_FAILURE() << "schema keyword not covered by this minimal validator: " << it.key()
                    << " at " << where;
    }
    if (it.key() == "properties") {
      for (auto prop = it->begin(); prop != it->end(); ++prop) {
        AssertOnlyCoveredKeywords(prop.value(), where + "/properties/" + prop.key());
      }
    }
  }
}

bool MatchesType(const nlohmann::json& value, const std::string& typeName) {
  if (typeName == "object") return value.is_object();
  if (typeName == "array") return value.is_array();
  if (typeName == "string") return value.is_string();
  if (typeName == "number") return value.is_number();
  if (typeName == "integer") return value.is_number_integer();
  if (typeName == "boolean") return value.is_boolean();
  if (typeName == "null") return value.is_null();
  ADD_FAILURE() << "unknown type name in schema: " << typeName;
  return false;
}

// Validates `value` against `schema`, appending human-readable errors.
// schemaDir anchors file-relative $ref resolution.
void Validate(const nlohmann::json& value, const nlohmann::json& schema,
              const std::filesystem::path& schemaDir, const std::string& where,
              std::vector<std::string>& errors) {
  if (schema.contains("$ref")) {
    // File-relative $ref (the only form the committed schemas use): load the
    // referenced schema from disk and validate against it in its own dir.
    const auto refPath =
        (schemaDir / schema.at("$ref").get<std::string>()).lexically_normal();
    const nlohmann::json refSchema = LoadJsonFile(refPath);
    AssertOnlyCoveredKeywords(refSchema, refPath.filename().string());
    Validate(value, refSchema, refPath.parent_path(), where, errors);
    return;
  }

  if (schema.contains("type")) {
    const nlohmann::json& type = schema.at("type");
    bool matched = false;
    if (type.is_string()) {
      matched = MatchesType(value, type.get<std::string>());
    } else {  // array of type names, e.g. ["number", "null"]
      for (const auto& t : type) {
        if (MatchesType(value, t.get<std::string>())) {
          matched = true;
          break;
        }
      }
    }
    if (!matched) {
      errors.push_back(where + ": type mismatch, wanted " + type.dump() + ", got " +
                       value.dump());
      return;  // further keyword checks on a wrongly-typed value are noise
    }
  }

  if (schema.contains("enum")) {
    bool found = false;
    for (const auto& candidate : schema.at("enum")) {
      if (candidate == value) {
        found = true;
        break;
      }
    }
    if (!found) {
      errors.push_back(where + ": " + value.dump() + " not in enum " +
                       schema.at("enum").dump());
    }
  }

  if (value.is_number()) {
    const double number = value.get<double>();
    if (schema.contains("minimum") && number < schema.at("minimum").get<double>()) {
      errors.push_back(where + ": " + value.dump() + " below minimum");
    }
    if (schema.contains("maximum") && number > schema.at("maximum").get<double>()) {
      errors.push_back(where + ": " + value.dump() + " above maximum");
    }
  }

  // Object keywords apply only to objects — a nullable object that is null
  // has already matched its "null" type alternative above.
  if (value.is_object()) {
    if (schema.contains("required")) {
      for (const auto& name : schema.at("required")) {
        if (!value.contains(name.get<std::string>())) {
          errors.push_back(where + ": missing required key " + name.dump());
        }
      }
    }
    if (schema.contains("properties")) {
      for (auto it = schema.at("properties").begin(); it != schema.at("properties").end();
           ++it) {
        if (value.contains(it.key())) {
          Validate(value.at(it.key()), it.value(), schemaDir, where + "/" + it.key(),
                   errors);
        }
      }
    }
  }
}

// Validates against the committed schema on disk and returns the error list.
std::vector<std::string> ValidateAgainstDiskSchema(const nlohmann::json& value) {
  const nlohmann::json schema = LoadJsonFile(kSchemaPath);
  AssertOnlyCoveredKeywords(schema, "cra-assessment-record.schema.json");
  std::vector<std::string> errors;
  Validate(value, schema, kSchemaPath.parent_path(), "record", errors);
  return errors;
}

std::string Join(const std::vector<std::string>& errors) {
  std::string out;
  for (const auto& e : errors) {
    out += e + "\n";
  }
  return out;
}

// ---------------------------------------------------------------------------
// Record builders
// ---------------------------------------------------------------------------

TrackedObject MakeRelayedSnapshot() {
  TrackedObject o;
  o.id = "v2x:1001:7";
  o.object_class = "vehicle";
  o.source = Source::v2x_relayed;
  o.position = {28.4, 0.4};
  o.distance = 28.4;
  o.speed = 8.3;
  o.confidence = 0.9;
  o.state = TrackState::tracked;
  o.timestamps = {1785801609550, 1785801609580, 1785801609600};
  return o;
}

AssessmentRecord MakeRecord() {
  AssessmentRecord r;
  r.trackId = "v2x:1001:7";
  r.warningType = "nlos_obstruction";
  r.riskState = "medium";
  r.riskStateEnteredMs = 1785801609400;
  r.firstSeenMs = 1785801608200;
  r.lastUpdatedMs = 1785801609600;
  r.distanceM = 46.5;
  r.previousDistanceM = 48.2;
  r.closingRateMps = 8.5;
  r.ttcS = 5.47;
  r.lastSnapshot = MakeRelayedSnapshot();
  r.lastKnownB = Vec2{18.1, 0.2};
  r.emittedCount = 1;
  r.rationale = "C tracked and d_AC 46.5 m <= RISK_NEAR_M";
  return r;
}

// The b_unknown shape: both nullable fields null (D5 — not closing, no B yet).
AssessmentRecord MakeRecordWithNulls() {
  AssessmentRecord r = MakeRecord();
  r.riskState = "low";
  r.ttcS = std::nullopt;
  r.lastKnownB = std::nullopt;
  r.rationale = "b_unknown";
  return r;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(AssessmentDb, RecordRoundTripsThroughUpsertAndGet) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  AssessmentDb db(log);

  const AssessmentRecord record = MakeRecord();
  db.upsert(record);

  const auto got = db.get(record.trackId, record.warningType);
  ASSERT_TRUE(got.has_value());
  // Field-exact equality, compared on the serialized form.
  EXPECT_EQ(nlohmann::json(*got), nlohmann::json(record));

  // The key is composite: same track under another plugin is a different row.
  EXPECT_FALSE(db.get(record.trackId, "some_other_plugin").has_value());
  EXPECT_FALSE(db.get("own:1", record.warningType).has_value());
}

TEST(AssessmentDb, UpsertReplacesUnderTheSameCompositeKey) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  AssessmentDb db(log);

  AssessmentRecord record = MakeRecord();
  db.upsert(record);
  record.riskState = "high";
  record.emittedCount = 2;
  db.upsert(record);

  ASSERT_EQ(db.all().size(), 1u);  // replaced, not duplicated
  const auto got = db.get(record.trackId, record.warningType);
  ASSERT_TRUE(got.has_value());
  EXPECT_EQ(got->riskState, "high");
  EXPECT_EQ(got->emittedCount, 2);
}

TEST(AssessmentDb, SerializedRecordValidatesAgainstTheCommittedSchema) {
  // Loaded from disk (kSchemaPath) — the schema is enforced, never restated.
  EXPECT_EQ(Join(ValidateAgainstDiskSchema(nlohmann::json(MakeRecord()))), "");
  // The nullable arms: ttcS and lastKnownB as JSON null must also validate.
  EXPECT_EQ(Join(ValidateAgainstDiskSchema(nlohmann::json(MakeRecordWithNulls()))), "");
}

TEST(AssessmentDb, ValidatorHasTeeth) {
  // Negative checks proving the minimal validator enforces the schema rather
  // than passing everything: wrong type, missing required key, bad enum value.
  nlohmann::json wrongType = MakeRecord();
  wrongType["ttcS"] = "soon";  // ["number", "null"] admits no string
  EXPECT_FALSE(ValidateAgainstDiskSchema(wrongType).empty());

  nlohmann::json missingKey = MakeRecord();
  missingKey.erase("trackId");
  EXPECT_FALSE(ValidateAgainstDiskSchema(missingKey).empty());

  nlohmann::json badEnum = MakeRecord();
  badEnum["riskState"] = "extreme";  // outside the frozen low|medium|high
  EXPECT_FALSE(ValidateAgainstDiskSchema(badEnum).empty());

  nlohmann::json badSnapshot = MakeRecord();
  badSnapshot["lastSnapshot"]["source"] = "telepathy";  // $ref is followed
  EXPECT_FALSE(ValidateAgainstDiskSchema(badSnapshot).empty());
}

TEST(AssessmentDb, EraseRemovesEveryWarningTypeForTheTrack) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  AssessmentDb db(log);

  AssessmentRecord first = MakeRecord();
  AssessmentRecord second = MakeRecord();
  second.warningType = "intersection_hazard";  // same track, another plugin
  AssessmentRecord other = MakeRecord();
  other.trackId = "own:1";  // another track, untouched by the erase
  db.upsert(first);
  db.upsert(second);
  db.upsert(other);
  ASSERT_EQ(db.all().size(), 3u);

  db.erase(first.trackId);

  EXPECT_FALSE(db.get(first.trackId, first.warningType).has_value());
  EXPECT_FALSE(db.get(second.trackId, second.warningType).has_value());
  ASSERT_EQ(db.all().size(), 1u);
  EXPECT_EQ(db.all()[0].trackId, "own:1");

  db.erase("never:seen");  // erasing an absent track is a no-op
  EXPECT_EQ(db.all().size(), 1u);
}

TEST(AssessmentDb, CommittedSampleDeserializesIntoTheStruct) {
  const nlohmann::json sample = LoadJsonFile(kSamplePath);
  const auto record = sample.get<AssessmentRecord>();

  EXPECT_EQ(record.trackId, "v2x:1001:7");
  EXPECT_EQ(record.warningType, "nlos_obstruction");
  EXPECT_EQ(record.riskState, "medium");
  ASSERT_TRUE(record.ttcS.has_value());
  EXPECT_DOUBLE_EQ(*record.ttcS, 5.47);
  EXPECT_EQ(record.lastSnapshot.source, Source::v2x_relayed);
  EXPECT_EQ(record.lastSnapshot.state, TrackState::tracked);
  ASSERT_TRUE(record.lastKnownB.has_value());
  EXPECT_DOUBLE_EQ(record.lastKnownB->x, 18.1);
  EXPECT_EQ(record.emittedCount, 1);

  // Semantic wire equality: the re-emitted JSON matches the committed sample.
  EXPECT_EQ(nlohmann::json(record), sample);
}

TEST(AssessmentDb, EveryUpsertAppendsOneAssessmentEvtLine) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(111, 222));
  AssessmentDb db(log);

  const AssessmentRecord record = MakeRecord();
  db.upsert(record);
  AssessmentRecord updated = record;
  updated.riskState = "high";
  db.upsert(updated);

  const auto out = lines(sink.str());
  ASSERT_EQ(out.size(), 2u);  // one line per write — the offline rebuild input
  EXPECT_EQ(log.count(events::kAssessment), 2u);

  const nlohmann::json first = parseEvtLine(out[0]);
  EXPECT_EQ(first.at("event"), "assessment");
  EXPECT_EQ(first.at("mono_ms"), 111);
  EXPECT_EQ(first.at("epoch_ms"), 222);
  // Payload = the serialized record, so the table is reconstructible offline.
  EXPECT_EQ(first.at("payload"), nlohmann::json(record));
  EXPECT_EQ(first.at("payload").get<AssessmentRecord>().trackId, record.trackId);

  const nlohmann::json second = parseEvtLine(out[1]);
  EXPECT_EQ(second.at("payload"), nlohmann::json(updated));
}

TEST(AssessmentDb, AllReturnsRecordsInDeterministicKeyOrder) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  AssessmentDb db(log);

  AssessmentRecord b = MakeRecord();
  b.trackId = "v2x:1001:9";
  AssessmentRecord a = MakeRecord();  // sorts first by trackId
  db.upsert(b);
  db.upsert(a);

  const auto all = db.all();
  ASSERT_EQ(all.size(), 2u);
  EXPECT_EQ(all[0].trackId, "v2x:1001:7");
  EXPECT_EQ(all[1].trackId, "v2x:1001:9");
}

}  // namespace
