#include <gtest/gtest.h>
#include "cra/risk_matrix.hpp"

TEST(RiskMatrixTest, DefaultThresholds) {
    ada::cra::RiskMatrix matrix;
    
    // Ego moving fast enough (10 m/s)
    EXPECT_EQ(matrix.evaluate_risk(1.0, 10.0, 10.0), ada::cra::ICWRiskLevel::CRITICAL);
    EXPECT_EQ(matrix.evaluate_risk(2.5, 20.0, 10.0), ada::cra::ICWRiskLevel::WARNING);
    EXPECT_EQ(matrix.evaluate_risk(4.0, 30.0, 10.0), ada::cra::ICWRiskLevel::INFO);
    EXPECT_EQ(matrix.evaluate_risk(6.0, 50.0, 10.0), ada::cra::ICWRiskLevel::NONE);
}

TEST(RiskMatrixTest, LowEgoSpeedSuppression) {
    ada::cra::RiskMatrix matrix;
    
    // Ego stationary (0.1 m/s < min_ego_speed 0.5 m/s)
    EXPECT_EQ(matrix.evaluate_risk(1.0, 5.0, 0.1), ada::cra::ICWRiskLevel::NONE);
}

TEST(RiskMatrixTest, JsonConfigLoading) {
    nlohmann::json j = {
        {"icw_config", {
            {"ttc_thresholds_sec", {
                {"critical", 1.0},
                {"warning", 2.0},
                {"info", 4.0}
            }},
            {"coasting_timeout_ms", 500},
            {"min_ego_speed_mps", 0.5},
            {"conflict_radius_meters", 5.0}
        }}
    };

    auto config = ada::cra::ICWRiskConfig::from_json(j);
    EXPECT_DOUBLE_EQ(config.ttc_critical_sec, 1.0);
    EXPECT_DOUBLE_EQ(config.ttc_warning_sec, 2.0);
    EXPECT_DOUBLE_EQ(config.ttc_info_sec, 4.0);
    EXPECT_EQ(config.coasting_timeout_ms, 500ULL);

    ada::cra::RiskMatrix matrix(config);
    EXPECT_EQ(matrix.evaluate_risk(1.5, 10.0, 10.0), ada::cra::ICWRiskLevel::WARNING);
}
