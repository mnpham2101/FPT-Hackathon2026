#pragma once

#include <optional>

#include "ada/track_store.hpp"
#include "ada/types.hpp"

namespace ada {

enum class RiskState {
    Clear,
    Warning,
};

const char* to_string(RiskState state);

struct RiskEvent {
    RiskState state = RiskState::Clear;
    TrackedObject object;
};

class CollisionRiskAssessor {
public:
    virtual ~CollisionRiskAssessor() = default;
    virtual std::optional<RiskEvent> assess(const TrackStore& store) = 0;
};

class NlosRiskAssessor final : public CollisionRiskAssessor {
public:
    explicit NlosRiskAssessor(double risk_distance_m);

    std::optional<RiskEvent> assess(const TrackStore& store) override;

private:
    double risk_distance_m_;
    RiskState last_state_ = RiskState::Clear;
    std::optional<TrackedObject> last_relayed_object_;
};

}  // namespace ada
