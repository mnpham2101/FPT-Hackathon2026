#ifndef ADA_ECU_CRA_I_COLLISION_RISK_ASSESSMENT_HPP
#define ADA_ECU_CRA_I_COLLISION_RISK_ASSESSMENT_HPP

// The R14 Collision Risk Assessment seam (HLD §6, Business logic; design
// decision D4) — header-only and FROZEN. The text below is transcribed from
// D4; later subtasks may not alter it without re-freezing.
//
// The plugin never emits (D4): assess() returns a RiskFinding and the output
// stage decides transport, which keeps rules replaceable independently of
// emission. Registration is explicit in src/cra/builtin_plugins.cpp, never
// static-init (D4).
//
// AssessmentDb is forward-declared here, not included — that is what lets
// this header land before the accessor exists and removes the circular
// dependency; the definition arrives with src/cra/assessment_db.hpp. The
// forward declaration carries no accessor signature.

#include <cstdint>
#include <optional>
#include <string>

#include "contracts/tracked_object.hpp"
#include "store/track_store.hpp"

namespace ada::cra {

class AssessmentDb;  // defined by src/cra/assessment_db.hpp, deliberately not included

using store::TrackStore;  // so the frozen D4 text below reads verbatim

struct RiskContext { const TrackStore& store; AssessmentDb& db; std::int64_t now_ms; };
struct RiskFinding {
  std::string warningType;                        // R4 registry key == plugin name
  std::string riskState;                          // low | medium | high (frozen R4)
  std::optional<contracts::TrackedObject> trigger;// R3 snapshot for the R4 `object` field
  std::string rationale;                          // evidence text for the database and the [EVT] stream
};
class ICollisionRiskAssessment {
 public:
  virtual ~ICollisionRiskAssessment() = default;
  virtual std::string name() const = 0;           // registry key
  virtual RiskFinding assess(RiskContext&) = 0;   // reads the store, reads and writes the database
};

}  // namespace ada::cra

#endif  // ADA_ECU_CRA_I_COLLISION_RISK_ASSESSMENT_HPP
