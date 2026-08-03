#include <gtest/gtest.h>
#include "contracts/r4_icw_payload.hpp"

TEST(ICWWarningPayloadTest, RoundtripSerialization) {
    ada::contracts::ICWWarningPayload original;
    original.warning_id = 42;
    original.warning_type = "ICW";
    original.risk_level = ada::cra::ICWRiskLevel::WARNING;
    original.target_object_id = 101;
    original.distance_m = 12.5;
    original.ttc_sec = 2.4;
    original.timestamp_ms = 1700000000000ULL;

    nlohmann::json j = original.to_json();
    EXPECT_EQ(j["warning_id"], 42);
    EXPECT_EQ(j["warning_type"], "ICW");
    EXPECT_EQ(j["risk_level"], "WARNING");
    EXPECT_EQ(j["target_object_id"], 101);
    EXPECT_DOUBLE_EQ(j["distance_m"], 12.5);
    EXPECT_DOUBLE_EQ(j["ttc_sec"], 2.4);
    EXPECT_EQ(j["timestamp_ms"], 1700000000000ULL);

    auto restored = ada::contracts::ICWWarningPayload::from_json(j);
    EXPECT_EQ(restored.warning_id, original.warning_id);
    EXPECT_EQ(restored.warning_type, original.warning_type);
    EXPECT_EQ(restored.risk_level, original.risk_level);
    EXPECT_EQ(restored.target_object_id, original.target_object_id);
    EXPECT_DOUBLE_EQ(restored.distance_m, original.distance_m);
    EXPECT_DOUBLE_EQ(restored.ttc_sec, original.ttc_sec);
    EXPECT_EQ(restored.timestamp_ms, original.timestamp_ms);
}
