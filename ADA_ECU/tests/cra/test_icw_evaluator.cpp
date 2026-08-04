#include <gtest/gtest.h>
#include "cra/icw_evaluator.hpp"

TEST(ICWEvaluatorTest, TrajectoryVectorIntersection) {
    // Ego moving along +X at 10 m/s from (0,0)
    // Target moving along -Y at 10 m/s from (20, 20)
    // Collision point at (20, 0)
    // Ego reaches (20,0) in t = 2.0s
    // Target reaches (20,0) in t = 2.0s
    auto cp = ada::cra::ICWEvaluator::compute_conflict_point(
        0.0, 0.0, 10.0, 0.0,
        20.0, 20.0, 0.0, -10.0
    );

    EXPECT_TRUE(cp.has_conflict);
    EXPECT_NEAR(cp.ttc_sec, 2.0, 1e-3);
    EXPECT_NEAR(cp.separation_min_m, 0.0, 1e-3);
}

TEST(ICWEvaluatorTest, RelayedCrossTrafficDetectionAndWarning) {
    ada::cra::ICWEvaluator evaluator;

    ada::cra::RelayedTrack vehicle_c;
    vehicle_c.object_id = 301;
    vehicle_c.pos_x_m = 20.0;
    vehicle_c.pos_y_m = 20.0;
    vehicle_c.vel_x_mps = 0.0;
    vehicle_c.vel_y_mps = -10.0; // Heading towards (20,0)
    vehicle_c.timestamp_ms = 1000;

    std::vector<ada::cra::RelayedTrack> tracks = {vehicle_c};

    // Ego moving +X at 10 m/s
    auto warning = evaluator.process_tracks(tracks, 10.0, 0.0, 1000);

    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->warning_type, "ICW");
    EXPECT_EQ(warning->target_object_id, 301);
    EXPECT_EQ(warning->risk_level, ada::cra::ICWRiskLevel::WARNING); // TTC = 2.0s (< 3.0s warning)
    EXPECT_NEAR(warning->ttc_sec, 2.0, 1e-3);
}

TEST(ICWEvaluatorTest, TrackCoastingAndExpiration) {
    ada::cra::ICWEvaluator evaluator;

    ada::cra::RelayedTrack vehicle_c;
    vehicle_c.object_id = 301;
    vehicle_c.pos_x_m = 20.0;
    vehicle_c.pos_y_m = 20.0;
    vehicle_c.vel_x_mps = 0.0;
    vehicle_c.vel_y_mps = -10.0;
    vehicle_c.timestamp_ms = 1000;

    // Initial detection -> WARNING
    evaluator.process_tracks({vehicle_c}, 10.0, 0.0, 1000);

    // Stream drops at t = 1200ms (200ms dt -> COASTING)
    auto warning_coasting = evaluator.process_tracks({}, 10.0, 0.0, 1200);
    ASSERT_TRUE(warning_coasting.has_value());
    EXPECT_EQ(warning_coasting->risk_level, ada::cra::ICWRiskLevel::WARNING);

    // Stream remains missing until t = 1600ms (> 500ms timeout -> EXPIRED & CLEAR)
    auto warning_clear = evaluator.process_tracks({}, 10.0, 0.0, 1600);
    ASSERT_TRUE(warning_clear.has_value());
    EXPECT_EQ(warning_clear->risk_level, ada::cra::ICWRiskLevel::NONE);
}

TEST(ICWEvaluatorTest, MultiTrackPrioritization) {
    ada::cra::ICWEvaluator evaluator;

    // Track 1: TTC = 4.0s (INFO)
    ada::cra::RelayedTrack track1;
    track1.object_id = 101;
    track1.pos_x_m = 40.0;
    track1.pos_y_m = 40.0;
    track1.vel_x_mps = 0.0;
    track1.vel_y_mps = -10.0;
    track1.timestamp_ms = 1000;

    // Track 2: TTC = 1.0s (CRITICAL)
    ada::cra::RelayedTrack track2;
    track2.object_id = 202;
    track2.pos_x_m = 10.0;
    track2.pos_y_m = 10.0;
    track2.vel_x_mps = 0.0;
    track2.vel_y_mps = -10.0;
    track2.timestamp_ms = 1000;

    std::vector<ada::cra::RelayedTrack> tracks = {track1, track2};

    auto warning = evaluator.process_tracks(tracks, 10.0, 0.0, 1000);

    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->target_object_id, 202); // Prioritizes Track 2 (CRITICAL)
    EXPECT_EQ(warning->risk_level, ada::cra::ICWRiskLevel::CRITICAL);
}
