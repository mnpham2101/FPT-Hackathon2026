#include "cra/plugins/chained_collision.hpp"

#include "cra/assessment_db.hpp"
#include "fusion/scene_composer.hpp"

namespace ada::cra {

namespace {

constexpr const char* kName = "nlos_obstruction";
constexpr const char* kLow = "low";
constexpr const char* kMedium = "medium";
constexpr const char* kHigh = "high";

// "C" = the nearest v2x_relayed track whose state is `tracked`. Only
// `tracked` counts; on an exact distance tie the smallest id wins
// (allBySource iterates in id order and the comparison is strict).
std::optional<contracts::TrackedObject> findTrackedC(const store::TrackStore& store) {
  std::optional<contracts::TrackedObject> nearest;
  for (const auto& track : store.allBySource(contracts::Source::v2x_relayed)) {
    if (track.state != contracts::TrackState::tracked) {
      continue;
    }
    if (!nearest || track.distance < nearest->distance) {
      nearest = track;
    }
  }
  return nearest;
}

}  // namespace

ChainedCollision::ChainedCollision(const config::Config& config)
    : riskNearM_(config.riskNearM),
      riskCriticalM_(config.riskCriticalM),
      riskTtcWarnS_(config.riskTtcWarnS),
      riskTtcCriticalS_(config.riskTtcCriticalS),
      riskDwellMs_(config.riskDwellMs),
      assessLogEveryMs_(config.assessLogEveryMs) {}

std::string ChainedCollision::name() const { return kName; }

RiskFinding ChainedCollision::assess(RiskContext& context) {
  const std::optional<contracts::TrackedObject> trackedC = findTrackedC(context.store);
  if (trackedC) {
    return assessTracked(context, *trackedC);
  }
  return decayToLow(context);
}

RiskFinding ChainedCollision::assessTracked(RiskContext& context,
                                            const contracts::TrackedObject& trackedC) {
  const std::optional<AssessmentRecord> stored = context.db.get(trackedC.id, kName);
  const std::optional<contracts::Vec2> fallbackB =
      stored ? stored->lastKnownB : std::nullopt;
  const std::optional<fusion::SceneGeometry> geometry = fusion::compose(
      context.store, std::optional<contracts::TrackedObject>(trackedC), fallbackB);
  if (!geometry) {
    // D5: no B, no chain. The exact rationale is load-bearing — the controller
    // matches it to emit assess_skipped_b_unknown. No record is written, so no
    // medium/high can ever be entered without a known B.
    return RiskFinding{kName, kLow, trackedC, "b_unknown"};
  }

  const double dAcM = geometry->vehicleC->x;  // has a value: trackedC was passed
  Working& working = working_[trackedC.id];

  // Closing rate over the per-track working samples; ttc only while closing.
  const double previousDAcM = working.prevDAcM.value_or(dAcM);
  double closingRateMps = 0.0;
  std::optional<double> ttcS;
  if (!working.prevDAcM) {
    working.prevDAcM = dAcM;
    working.prevNowMs = context.now_ms;
  } else {
    const std::int64_t deltaMs = context.now_ms - working.prevNowMs;
    if (deltaMs > 0) {  // Δ == 0 keeps the earlier sample — no rate computable yet
      closingRateMps = -(dAcM - *working.prevDAcM) * 1000.0 / static_cast<double>(deltaMs);
      if (closingRateMps > 0.0) {
        ttcS = dAcM / closingRateMps;
      }
      working.prevDAcM = dAcM;
      working.prevNowMs = context.now_ms;
    }
  }

  // D5's total, ordered band table — the first matching row wins; within a
  // row the range clause names the rationale before the ttc clause.
  std::string level = kLow;
  std::string rationale = "clear";
  if (dAcM <= riskCriticalM_) {
    level = kHigh;
    rationale = "range";
  } else if (ttcS && *ttcS <= riskTtcCriticalS_) {
    level = kHigh;
    rationale = "ttc";
  } else if (dAcM <= riskNearM_) {
    level = kMedium;
    rationale = "range";
  } else if (ttcS && *ttcS <= riskTtcWarnS_) {
    level = kMedium;
    rationale = "ttc";
  }

  const auto fill = [&](AssessmentRecord& record, const std::string& why) {
    record.distanceM = dAcM;
    record.previousDistanceM = previousDAcM;
    record.closingRateMps = closingRateMps;
    record.ttcS = ttcS;
    record.lastSnapshot = trackedC;
    record.lastKnownB = geometry->vehicleB;  // (d_AB, y_B) whenever B is known
    record.lastUpdatedMs = context.now_ms;
    record.rationale = why;
  };

  AssessmentRecord record;
  if (stored) {
    record = *stored;
  } else {
    // Cadence rule (a): first creation for a newly-assessed tracked C, at low.
    record.trackId = trackedC.id;
    record.warningType = kName;
    record.riskState = kLow;
    record.riskStateEnteredMs = context.now_ms;
    record.firstSeenMs = context.now_ms;
    record.emittedCount = 0;
    fill(record, "new_track");
    context.db.upsert(record);
    working.lastUpsertMs = context.now_ms;
    working.candidateLevel = kLow;
    working.candidateSinceMs = context.now_ms;
  }

  if (level != working.candidateLevel) {
    working.candidateLevel = level;
    working.candidateSinceMs = context.now_ms;
  }
  if (working.candidateLevel != record.riskState) {
    // Cadence rule (b): a committed riskState change, either direction.
    record.riskState = working.candidateLevel;
    record.riskStateEnteredMs = context.now_ms;
    fill(record, rationale);
    context.db.upsert(record);
    working.lastUpsertMs = context.now_ms;
  } else if (assessLogEveryMs_ > 0 &&
             context.now_ms - working.lastUpsertMs >= assessLogEveryMs_) {
    // Cadence rule (c): the heartbeat.
    fill(record, rationale);
    context.db.upsert(record);
    working.lastUpsertMs = context.now_ms;
  }

  return RiskFinding{kName, record.riskState, trackedC, rationale};
}

RiskFinding ChainedCollision::decayToLow(RiskContext& context) {
  // No tracked C: every previously-assessed track decays toward low. The
  // record survives the store erasure (D4); its lastSnapshot is the trigger.
  RiskFinding finding{kName, kLow, std::nullopt, "no_tracked_c"};
  for (auto& [trackId, working] : working_) {
    const std::optional<AssessmentRecord> stored = context.db.get(trackId, kName);
    if (!stored) {
      continue;
    }
    // A later re-admission must not difference d_AC across the gap.
    working.prevDAcM.reset();

    AssessmentRecord record = *stored;
    if (working.candidateLevel != kLow) {
      working.candidateLevel = kLow;
      working.candidateSinceMs = context.now_ms;
    }
    if (record.riskState != kLow) {
      // The return to low is a committed transition like any other (D5).
      record.riskState = kLow;
      record.riskStateEnteredMs = context.now_ms;
      record.closingRateMps = 0.0;
      record.ttcS = std::nullopt;
      record.lastUpdatedMs = context.now_ms;
      record.rationale = "no_tracked_c";
      context.db.upsert(record);
      working.lastUpsertMs = context.now_ms;
    } else if (assessLogEveryMs_ > 0 &&
               context.now_ms - working.lastUpsertMs >= assessLogEveryMs_) {
      record.lastUpdatedMs = context.now_ms;
      record.rationale = "no_tracked_c";
      context.db.upsert(record);
      working.lastUpsertMs = context.now_ms;
    }

    if (!finding.trigger) {
      finding.riskState = record.riskState;
      finding.trigger = record.lastSnapshot;
    }
  }
  return finding;
}

}  // namespace ada::cra
