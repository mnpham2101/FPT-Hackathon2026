#pragma once

#include <optional>

#include "ada/track_store.hpp"
#include "ada/types.hpp"

namespace ada {

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

    std::optional<RiskEvent> assess(const TrackStore& store, std::int64_t now_ms) override;

private:
    RiskState classify(const std::optional<TrackedObject>& relayed) const;

    double near_m_;
    double critical_m_;
    std::int64_t dwell_ms_;
    RiskState last_state_ = RiskState::Low;
    RiskState pending_state_ = RiskState::Low;
    std::int64_t pending_since_ms_ = 0;
    std::optional<TrackedObject> last_relayed_object_;
};

}  // namespace ada
