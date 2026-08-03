#include "ada/risk_assessor.hpp"

namespace ada {

const char* to_string(RiskState state) {
    switch (state) {
        case RiskState::Low:
            return "low";
        case RiskState::Medium:
            return "medium";
        case RiskState::High:
            return "high";
    }
    return "low";
}

NlosRiskAssessor::NlosRiskAssessor(double near_m, double critical_m, std::int64_t dwell_ms)
    : near_m_(near_m), critical_m_(critical_m), dwell_ms_(dwell_ms) {}

RiskState NlosRiskAssessor::classify(const std::optional<TrackedObject>& relayed) const {
    if (!relayed || relayed->state != TrackState::Tracked) {
        return RiskState::Low;
    }
    if (relayed->distance_m <= critical_m_) {
        return RiskState::High;
    }
    if (relayed->distance_m <= near_m_) {
        return RiskState::Medium;
    }
    return RiskState::Low;
}

std::optional<RiskEvent> NlosRiskAssessor::assess(const TrackStore& store, std::int64_t now_ms) {
    const auto relayed = store.nearest(Source::V2xRelayed);
    const auto latest_relayed = store.latest(Source::V2xRelayed);
    if (latest_relayed) {
        last_relayed_object_ = *latest_relayed;
    }

    const auto current = classify(relayed);

    if (current == last_state_) {
        pending_state_ = current;
        pending_since_ms_ = now_ms;
        return std::nullopt;
    }

    if (current != pending_state_) {
        pending_state_ = current;
        pending_since_ms_ = now_ms;
    }
    if (now_ms - pending_since_ms_ < dwell_ms_) {
        return std::nullopt;
    }

    last_state_ = current;
    if (relayed) {
        return RiskEvent{current, *relayed, relayed->distance_m, "relayed_distance_threshold"};
    }
    if (last_relayed_object_) {
        return RiskEvent{current, *last_relayed_object_, last_relayed_object_->distance_m, "relayed_track_unavailable"};
    }
    return std::nullopt;
}

}  // namespace ada
