#include "cra/risk_matrix.hpp"
#include <fstream>
#include <stdexcept>

namespace ada::cra {

ICWRiskConfig ICWRiskConfig::from_json(const nlohmann::json& j) {
    ICWRiskConfig cfg;
    if (j.contains("icw_config")) {
        const auto& icw = j.at("icw_config");
        if (icw.contains("ttc_thresholds_sec")) {
            const auto& ttc = icw.at("ttc_thresholds_sec");
            cfg.ttc_critical_sec = ttc.value("critical", 1.5);
            cfg.ttc_warning_sec = ttc.value("warning", 3.0);
            cfg.ttc_info_sec = ttc.value("info", 5.0);
        }
        cfg.coasting_timeout_ms = icw.value("coasting_timeout_ms", 500ULL);
        cfg.min_ego_speed_mps = icw.value("min_ego_speed_mps", 0.5);
        cfg.conflict_radius_meters = icw.value("conflict_radius_meters", 5.0);
    }
    return cfg;
}

ICWRiskConfig ICWRiskConfig::load_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return ICWRiskConfig{};
    }
    nlohmann::json j;
    ifs >> j;
    return from_json(j);
}

RiskMatrix::RiskMatrix(const ICWRiskConfig& config) : config_(config) {}

void RiskMatrix::set_config(const ICWRiskConfig& config) {
    config_ = config;
}

const ICWRiskConfig& RiskMatrix::get_config() const {
    return config_;
}

ICWRiskLevel RiskMatrix::evaluate_risk(double ttc_sec, double distance_m, double ego_speed_mps) const {
    // Suppress warnings when ego is stationary below minimum speed threshold
    if (ego_speed_mps < config_.min_ego_speed_mps) {
        return ICWRiskLevel::NONE;
    }

    if (ttc_sec < 0.0 || ttc_sec > config_.ttc_info_sec) {
        return ICWRiskLevel::NONE;
    }

    if (ttc_sec < config_.ttc_critical_sec) {
        return ICWRiskLevel::CRITICAL;
    } else if (ttc_sec < config_.ttc_warning_sec) {
        return ICWRiskLevel::WARNING;
    } else if (ttc_sec < config_.ttc_info_sec) {
        return ICWRiskLevel::INFO;
    }

    return ICWRiskLevel::NONE;
}

} // namespace ada::cra
