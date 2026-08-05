#ifndef ADA_ECU_CRA_ASSESSMENT_DB_HPP
#define ADA_ECU_CRA_ASSESSMENT_DB_HPP

// The R14 assessment database accessor (HLD decision D4; subtask 14.2.5.3):
// a typed in-process record table over schema/cra-assessment-record.schema.json
// — the seam a future milestone swaps for real persistence. Deliberately NO
// database engine; D4 records the rationale and this file does not revisit it.
//
// This header DEFINES the `class AssessmentDb` that
// src/cra/i_collision_risk_assessment.hpp forward-declares in ada::cra. The
// interface header is deliberately not included: completing the forward
// declaration needs only the matching name and namespace, and not including it
// keeps this library free of the store dependency the interface pulls in.
//
// Key: (trackId, warningType) — D4's field table ("which track, assessed by
// which plugin"), per plans/phase2_tasks.md § Open items item 5. A
// reconciliation there may change get()'s signature and nothing else.
//
// Every upsert is also appended to the [EVT] stream as an `assessment` line,
// payload = the serialized record, so the table is reconstructible offline
// (D4, D8). Threading: per D2 the main thread is the single writer of this
// database, so the table itself takes no lock; the injected EventLog is
// thread-safe on its own.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "contracts/tracked_object.hpp"
#include "log/event_log.hpp"

namespace ada::cra {

// One row of the assessment database — mirrors the fourteen fields of
// schema/cra-assessment-record.schema.json exactly, by name. Field meanings
// live in the schema and D4's field table, not restated here.
struct AssessmentRecord {
  std::string trackId;
  std::string warningType;
  std::string riskState;  // frozen R4 vocabulary: low | medium | high
  std::int64_t riskStateEnteredMs{};
  std::int64_t firstSeenMs{};
  std::int64_t lastUpdatedMs{};
  double distanceM{};
  double previousDistanceM{};
  double closingRateMps{};
  std::optional<double> ttcS;  // schema type ["number", "null"]
  contracts::TrackedObject lastSnapshot;
  std::optional<contracts::Vec2> lastKnownB;  // schema type ["object", "null"]
  std::int64_t emittedCount{};
  std::string rationale;
};

// nlohmann serialization matching the schema field names exactly; nullopt
// serializes as JSON null for the two nullable fields.
void to_json(nlohmann::json& j, const AssessmentRecord& r);
void from_json(const nlohmann::json& j, AssessmentRecord& r);

class AssessmentDb {
 public:
  // eventLog: the R18 [EVT] writer every upsert is mirrored to. Held by
  // reference for the accessor's lifetime; the caller keeps it alive.
  explicit AssessmentDb(log::EventLog& eventLog);

  AssessmentDb(const AssessmentDb&) = delete;
  AssessmentDb& operator=(const AssessmentDb&) = delete;

  // The record for one (trackId, warningType) pair; nullopt when absent.
  std::optional<AssessmentRecord> get(const std::string& trackId,
                                      const std::string& warningType) const;

  // Inserts or replaces the record under (record.trackId, record.warningType),
  // then emits one `assessment` [EVT] line whose payload is the serialized
  // record (D4: the table is reconstructible offline from the stream).
  void upsert(const AssessmentRecord& record);

  // Erases every warningType's record for the track. No-op when absent.
  void erase(const std::string& trackId);

  // Every record, in deterministic (trackId, warningType) key order.
  std::vector<AssessmentRecord> all() const;

 private:
  using Key = std::pair<std::string, std::string>;  // (trackId, warningType)

  log::EventLog& eventLog_;
  std::map<Key, AssessmentRecord> records_;
};

}  // namespace ada::cra

#endif  // ADA_ECU_CRA_ASSESSMENT_DB_HPP
