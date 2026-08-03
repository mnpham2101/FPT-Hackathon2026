#ifndef ADA_CRA_RISK_MATRIX_HPP
#define ADA_CRA_RISK_MATRIX_HPP

#include "cra/icw_conflict_point.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace ada::cra {

struct ICWRiskConfig {
    double ttc_critical_sec{1.5};
    double ttc_warning_sec{3.0};
    double ttc_info_sec{5.0};
    uint64_t coasting_timeout_ms{500};
    double min_ego_speed_mps{0.5};
    double conflict_radius_meters{5.0};

    static ICWRiskConfig load_from_file(const std::string& filepath);
    static ICWRiskConfig from_json(const nlohmann::json& j);
};

class RiskMatrix {
public:
    explicit RiskMatrix(const ICWRiskConfig& config = ICWRiskConfig{});

    void set_config(const ICWRiskConfig& config);
    const ICWRiskConfig& get_config() const;

    ICWRiskLevel evaluate_risk(double ttc_sec, double distance_m, double ego_speed_mps) const;

private:
    ICWRiskConfig config_;
};

} // namespace ada::cra

#endif // ADA_CRA_RISK_MATRIX_HPP
