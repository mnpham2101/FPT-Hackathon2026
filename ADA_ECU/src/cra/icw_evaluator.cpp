#include "cra/icw_evaluator.hpp"
#include <cmath>
#include <algorithm>

namespace ada::cra {

ICWEvaluator::ICWEvaluator(const RiskMatrix& risk_matrix) : risk_matrix_(risk_matrix) {}

void ICWEvaluator::set_risk_matrix(const RiskMatrix& risk_matrix) {
    risk_matrix_ = risk_matrix;
}

ICWConflictPoint ICWEvaluator::compute_conflict_point(
    double ego_x, double ego_y, double ego_vx, double ego_vy,
    double target_x, double target_y, double target_vx, double target_vy) 
{
    ICWConflictPoint result;
    
    // Relative position: P_rel = P_target - P_ego
    double rx = target_x - ego_x;
    double ry = target_y - ego_y;
    
    // Relative velocity: V_rel = V_target - V_ego
    double vx = target_vx - ego_vx;
    double vy = target_vy - ego_vy;

    double v_sq = vx * vx + vy * vy;
    if (v_sq < 1e-6) {
        // Parallel or relative stationary
        result.has_conflict = false;
        result.distance_m = std::hypot(rx, ry);
        return result;
    }

    // Time of Closest Approach: t_cpa = - (P_rel . V_rel) / |V_rel|^2
    double t_cpa = -(rx * vx + ry * vy) / v_sq;

    if (t_cpa < 0.0) {
        // Target moving away
        result.has_conflict = false;
        result.ttc_sec = 999.0;
        result.distance_m = std::hypot(rx, ry);
        return result;
    }

    // Minimum distance at t_cpa
    double min_x = rx + vx * t_cpa;
    double min_y = ry + vy * t_cpa;
    double d_min = std::hypot(min_x, min_y);

    result.ttc_sec = t_cpa;
    result.distance_m = std::hypot(rx, ry);
    result.separation_min_m = d_min;
    result.has_conflict = (d_min <= 5.0); // Within conflict radius

    return result;
}

std::optional<contracts::ICWWarningPayload> ICWEvaluator::process_tracks(
    const std::vector<RelayedTrack>& incoming_tracks,
    double ego_vx, double ego_vy,
    uint64_t current_time_ms) 
{
    double ego_speed = std::hypot(ego_vx, ego_vy);
    const auto& config = risk_matrix_.get_config();

    // 1. Update active tracks map & state machine
    for (const auto& incoming : incoming_tracks) {
        auto it = active_tracks_.find(incoming.object_id);
        if (it != active_tracks_.end()) {
            it->second = incoming;
            it->second.state = TrackState::ACTIVE;
            it->second.timestamp_ms = current_time_ms;
        } else {
            RelayedTrack track = incoming;
            track.state = TrackState::ACTIVE;
            track.timestamp_ms = current_time_ms;
            active_tracks_[incoming.object_id] = track;
        }
    }

    // 2. Process coasting & expiration (T009)
    std::vector<uint32_t> to_remove;
    for (auto& [id, track] : active_tracks_) {
        uint64_t dt_ms = (current_time_ms > track.timestamp_ms) ? (current_time_ms - track.timestamp_ms) : 0;
        
        if (dt_ms > config.coasting_timeout_ms) {
            track.state = TrackState::EXPIRED;
            to_remove.push_back(id);
        } else if (dt_ms > 100) {
            // Coasting: dead reckoning update
            track.state = TrackState::COASTING;
            double dt_sec = dt_ms / 1000.0;
            track.pos_x_m += track.vel_x_mps * (dt_sec - 0.1); // Incremental project
            track.pos_y_m += track.vel_y_mps * (dt_sec - 0.1);
        }
    }

    for (uint32_t id : to_remove) {
        active_tracks_.erase(id);
    }

    // 3. Multi-track hazard evaluation & prioritization (T015)
    struct EvaluatedHazard {
        uint32_t object_id;
        ICWRiskLevel risk_level;
        double ttc_sec;
        double distance_m;
    };

    std::vector<EvaluatedHazard> hazards;

    for (const auto& [id, track] : active_tracks_) {
        auto cp = compute_conflict_point(0.0, 0.0, ego_vx, ego_vy,
                                         track.pos_x_m, track.pos_y_m, track.vel_x_mps, track.vel_y_mps);

        if (!cp.has_conflict) {
            continue;
        }

        ICWRiskLevel risk = risk_matrix_.evaluate_risk(cp.ttc_sec, cp.distance_m, ego_speed);
        if (risk != ICWRiskLevel::NONE) {
            hazards.push_back({id, risk, cp.ttc_sec, cp.distance_m});
        }
    }

    // Sort hazards by highest risk level, then lowest TTC
    std::sort(hazards.begin(), hazards.end(), [](const EvaluatedHazard& a, const EvaluatedHazard& b) {
        if (a.risk_level != b.risk_level) {
            return static_cast<int>(a.risk_level) > static_cast<int>(b.risk_level);
        }
        return a.ttc_sec < b.ttc_sec;
    });

    // 4. Output selection & De-escalation (T013)
    if (!hazards.empty()) {
        const auto& top_hazard = hazards.front();
        contracts::ICWWarningPayload payload;
        payload.warning_id = warning_sequence_id_++;
        payload.warning_type = "ICW";
        payload.risk_level = top_hazard.risk_level;
        payload.target_object_id = top_hazard.object_id;
        payload.distance_m = top_hazard.distance_m;
        payload.ttc_sec = top_hazard.ttc_sec;
        payload.timestamp_ms = current_time_ms;

        previous_emitted_risk_ = top_hazard.risk_level;
        previous_target_id_ = top_hazard.object_id;

        return payload;
    } else if (previous_emitted_risk_ != ICWRiskLevel::NONE) {
        // De-escalation clearance event
        contracts::ICWWarningPayload clear_payload;
        clear_payload.warning_id = warning_sequence_id_++;
        clear_payload.warning_type = "ICW";
        clear_payload.risk_level = ICWRiskLevel::NONE;
        clear_payload.target_object_id = previous_target_id_;
        clear_payload.distance_m = 0.0;
        clear_payload.ttc_sec = 0.0;
        clear_payload.timestamp_ms = current_time_ms;

        previous_emitted_risk_ = ICWRiskLevel::NONE;
        previous_target_id_ = 0;

        return clear_payload;
    }

    return std::nullopt;
}

} // namespace ada::cra
