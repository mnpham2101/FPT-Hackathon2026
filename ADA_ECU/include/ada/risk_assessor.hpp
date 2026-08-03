#pragma once

#include <memory>
#include <optional>

#include "ada/track_store.hpp"
#include "ada/types.hpp"
#include "ada/scene_composer.hpp"

namespace ada {

class AssessmentDb;

enum class RiskState {
    Low,
    Medium,
    High,
};

const char* to_string(RiskState state);

struct RiskEvent {
    RiskState state = RiskState::Low;
    TrackedObject object;
    double distance_m = 0.0;
    std::optional<double> ttc_s;
    SceneGeometry geometry;
    bool has_current_c = false;
    std::string rationale;
};

class CollisionRiskAssessor {
public:
    virtual ~CollisionRiskAssessor() = default;
    virtual std::optional<RiskEvent> assess(const TrackStore& store, std::int64_t now_ms) = 0;
};

class NlosRiskAssessor final : public CollisionRiskAssessor {
public:
    NlosRiskAssessor(double near_m, double critical_m, std::int64_t dwell_ms);
    NlosRiskAssessor(double near_m, double critical_m, std::int64_t dwell_ms, std::shared_ptr<AssessmentDb> db);

    std::optional<RiskEvent> assess(const TrackStore& store, std::int64_t now_ms) override;

private:
    RiskState classify(double distance_ac_m, const std::optional<double>& ttc_s) const;

    double near_m_;
    double critical_m_;
    std::int64_t dwell_ms_;
    RiskState pending_state_ = RiskState::Low;
    std::int64_t pending_since_ms_ = 0;
    std::optional<TrackedObject> last_relayed_object_;
    std::optional<SceneGeometry> last_scene_;
    std::shared_ptr<AssessmentDb> db_;
};

}  // namespace ada
