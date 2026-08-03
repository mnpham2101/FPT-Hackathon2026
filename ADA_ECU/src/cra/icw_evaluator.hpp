#ifndef ADA_CRA_ICW_EVALUATOR_HPP
#define ADA_CRA_ICW_EVALUATOR_HPP

#include "cra/relayed_track.hpp"
#include "cra/icw_conflict_point.hpp"
#include "cra/risk_matrix.hpp"
#include "contracts/r4_icw_payload.hpp"
#include <vector>
#include <unordered_map>
#include <optional>

namespace ada::cra {

class ICWEvaluator {
public:
    explicit ICWEvaluator(const RiskMatrix& risk_matrix = RiskMatrix{});

    void set_risk_matrix(const RiskMatrix& risk_matrix);

    // Compute 2D vector trajectory intersection & TTC between ego and target
    static ICWConflictPoint compute_conflict_point(
        double ego_pos_x, double ego_pos_y, double ego_vel_x, double ego_vel_y,
        double target_pos_x, double target_pos_y, double target_vel_x, double target_vel_y);

    // Process incoming relayed tracks frame and return prioritized ICW warning payload (if any)
    std::optional<contracts::ICWWarningPayload> process_tracks(
        const std::vector<RelayedTrack>& incoming_tracks,
        double ego_vel_x, double ego_vel_y,
        uint64_t current_time_ms);

private:
    RiskMatrix risk_matrix_;
    std::unordered_map<uint32_t, RelayedTrack> active_tracks_;
    uint32_t warning_sequence_id_{1};
    ICWRiskLevel previous_emitted_risk_{ICWRiskLevel::NONE};
    uint32_t previous_target_id_{0};
};

} // namespace ada::cra

#endif // ADA_CRA_ICW_EVALUATOR_HPP
