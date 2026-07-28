#include "ada/risk_assessor.hpp"

namespace ada {

const char* to_string(RiskState state) {
    return state == RiskState::Warning ? "warning" : "clear";
}

NlosRiskAssessor::NlosRiskAssessor(double risk_distance_m) : risk_distance_m_(risk_distance_m) {}

std::optional<RiskEvent> NlosRiskAssessor::assess(const TrackStore& store) {
    const auto relayed = store.nearest(Source::V2xRelayed);
    const auto latest_relayed = store.latest(Source::V2xRelayed);
    if (latest_relayed) {
        last_relayed_object_ = *latest_relayed;
    }

    const auto current = relayed && relayed->state == TrackState::Tracked && relayed->distance_m <= risk_distance_m_
                             ? RiskState::Warning
                             : RiskState::Clear;

    if (current == last_state_) {
        return std::nullopt;
    }

    last_state_ = current;
    if (relayed) {
        return RiskEvent{current, *relayed};
    }
    if (last_relayed_object_) {
        return RiskEvent{current, *last_relayed_object_};
    }
    return std::nullopt;
}

}  // namespace ada
