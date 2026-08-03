#include "ada/risk_assessor.hpp"

#include <memory>
#include <stdexcept>

#include "ada/assessment_db.hpp"

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
    : NlosRiskAssessor(near_m, critical_m, dwell_ms, std::make_shared<AssessmentDb>()) {}

NlosRiskAssessor::NlosRiskAssessor(
    double near_m,
    double critical_m,
    std::int64_t dwell_ms,
    std::shared_ptr<AssessmentDb> db)
    : near_m_(near_m), critical_m_(critical_m), dwell_ms_(dwell_ms), db_(std::move(db)) {
    if (!db_) {
        throw std::runtime_error("NLOS assessor requires an assessment database");
    }
}

RiskState NlosRiskAssessor::classify(double distance_ac_m, const std::optional<double>& ttc_s) const {
    if (distance_ac_m <= critical_m_ || (ttc_s && *ttc_s <= 3.0)) {
        return RiskState::High;
    }
    if (distance_ac_m <= near_m_) {
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

    const auto scene = compose_scene(store, relayed);
    if (!scene) {
        return std::nullopt;
    }
    if (relayed) {
        last_scene_ = scene;
    }

    const auto assessed_object = relayed ? relayed : last_relayed_object_;
    if (!assessed_object) {
        return std::nullopt;
    }
    const auto previous = db_->get("nlos_obstruction", assessed_object->id);
    std::optional<double> ttc_s;
    if (relayed && previous && now_ms > previous->timestamp_ms) {
        const auto elapsed_s = static_cast<double>(now_ms - previous->timestamp_ms) / 1000.0;
        const auto closing_rate = (previous->distance_m - scene->distance_ac_m) / elapsed_s;
        if (closing_rate > 0.0) {
            ttc_s = scene->distance_ac_m / closing_rate;
        }
    }
    const auto current = relayed ? classify(scene->distance_ac_m, ttc_s) : RiskState::Low;
    const auto committed = previous ? previous->risk_state : RiskState::Low;
    const auto rationale = relayed ? "composed_distance_threshold" : "relayed_track_unavailable";

    const auto persist = [&](RiskState state) {
        db_->upsert(AssessmentRecord{
            now_ms,
            "nlos_obstruction",
            assessed_object->id,
            scene->distance_ac_m,
            state,
            rationale,
        });
    };

    if (current == committed) {
        pending_state_ = current;
        pending_since_ms_ = now_ms;
        persist(committed);
        return std::nullopt;
    }

    if (current != pending_state_) {
        pending_state_ = current;
        pending_since_ms_ = now_ms;
    }
    if (now_ms - pending_since_ms_ < dwell_ms_) {
        persist(committed);
        return std::nullopt;
    }

    persist(current);
    if (relayed) {
        return RiskEvent{current, *relayed, scene->distance_ac_m, ttc_s, *scene, true, rationale};
    }
    if (last_relayed_object_ && last_scene_) {
        auto clear_scene = *last_scene_;
        clear_scene.vehicle_c = std::nullopt;
        return RiskEvent{
            current,
            *last_relayed_object_,
            last_scene_->distance_ac_m,
            std::nullopt,
            clear_scene,
            false,
            rationale,
        };
    }
    return std::nullopt;
}

}  // namespace ada
