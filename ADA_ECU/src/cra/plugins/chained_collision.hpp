#ifndef ADA_ECU_CRA_PLUGINS_CHAINED_COLLISION_HPP
#define ADA_ECU_CRA_PLUGINS_CHAINED_COLLISION_HPP

// The M1 NLOS plugin (HLD §6, Business logic; design decisions D4, D5) — the
// figure's "Chained Collision", registered under the frozen R4 registry key
// nlos_obstruction. assess() reads the store, reads and writes the assessment
// database, and returns the COMMITTED level; it never emits — the output
// stage decides transport (D4).
//
// D5's band table is total and ordered — the first matching row wins:
//   1  high    C tracked and (d_AC ≤ RISK_CRITICAL_M or ttc ≤ RISK_TTC_CRITICAL_S)
//   2  medium  C tracked and (d_AC ≤ RISK_NEAR_M     or ttc ≤ RISK_TTC_WARN_S)
//   3  low     everything else — no tracked C, b_unknown, or C tracked but
//              neither near nor closing fast
// "C" is the nearest v2x_relayed track whose state is `tracked` — only
// `tracked` counts. d_AC is the composed range from fusion/scene_composer;
// the geometry is never duplicated here. With no own-sensor B and no
// remembered lastKnownB, d_AC does not exist: the finding is low with the
// exact rationale "b_unknown", which the controller matches to emit
// assess_skipped_b_unknown — no medium/high is ever entered without a known
// B (D5).
//
// A candidate level must hold RISK_DWELL_MS before it commits — one debounce
// across all three thresholds, independent of the R13 gate hysteresis. Every
// committed change, downgrades and the return to low included, changes the
// returned riskState exactly once; steady state changes nothing (D5).
//
// Database cadence (D4 reconciled with D8): db.upsert() — which IS the [EVT]
// assessment line — happens only on (a) first creation of a newly-assessed
// tracked C's record, at low; (b) every committed riskState change; (c) an
// ASSESS_LOG_EVERY_MS heartbeat per (trackId, warningType). The record
// survives C's erasure, and its lastSnapshot is the trigger from then on.
//
// Clock rule (D10): every *_Ms value here — RiskContext.now_ms, the record's
// millisecond fields, the dwell arithmetic — is CLOCK_MONOTONIC milliseconds,
// the fusion tick's domain. The plugin reads no clock of its own.

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "config/config.hpp"
#include "cra/i_collision_risk_assessment.hpp"

namespace ada::cra {

class ChainedCollision : public ICollisionRiskAssessment {
 public:
  // Copies the five D5 risk values plus the D8 heartbeat cadence out of the
  // loaded Config; the reference is not held.
  explicit ChainedCollision(const config::Config& config);

  std::string name() const override;  // exactly "nlos_obstruction"

  RiskFinding assess(RiskContext& context) override;

 private:
  // Per-track internal working state, sampled each assess() call — never
  // serialized; the record carries the evidence values.
  struct Working {
    std::optional<double> prevDAcM;     // the prior composed-range sample
    std::int64_t prevNowMs = 0;         // when it was sampled (now_ms domain)
    std::string candidateLevel;         // instantaneous level awaiting dwell
    std::int64_t candidateSinceMs = 0;  // when that candidate first appeared
    std::int64_t lastUpsertMs = 0;      // the heartbeat anchor
  };

  RiskFinding assessTracked(RiskContext& context, const contracts::TrackedObject& trackedC);
  RiskFinding decayToLow(RiskContext& context);

  const double riskNearM_;
  const double riskCriticalM_;
  const double riskTtcWarnS_;
  const double riskTtcCriticalS_;
  const std::int64_t riskDwellMs_;
  const std::int64_t assessLogEveryMs_;

  std::map<std::string, Working> working_;  // keyed by trackId
};

}  // namespace ada::cra

#endif  // ADA_ECU_CRA_PLUGINS_CHAINED_COLLISION_HPP
