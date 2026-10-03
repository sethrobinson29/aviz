#include "visuals/flow_field/flow_field_visual.h"

#include <cmath>
#include <limits>
#include <random>

#include <gtest/gtest.h>

namespace {
float length(Vector2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
}

TEST(Hash2dTest, IsDeterministic) {
    EXPECT_EQ(flow_field_math::hash2d(3, 7), flow_field_math::hash2d(3, 7));
}

TEST(Hash2dTest, DifferentInputsLikelyDiffer) {
    EXPECT_NE(flow_field_math::hash2d(3, 7), flow_field_math::hash2d(7, 3));
    EXPECT_NE(flow_field_math::hash2d(0, 0), flow_field_math::hash2d(1, 0));
}

TEST(ValueNoise2dTest, IsDeterministic) {
    float a = flow_field_math::value_noise2d(1.25f, -3.5f);
    float b = flow_field_math::value_noise2d(1.25f, -3.5f);
    EXPECT_FLOAT_EQ(a, b);
}

TEST(ValueNoise2dTest, StaysWithinUnitRange) {
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 5; ++y) {
            float n = flow_field_math::value_noise2d(static_cast<float>(x) * 0.37f,
                                                       static_cast<float>(y) * 0.61f);
            EXPECT_GE(n, -1.0f);
            EXPECT_LE(n, 1.0f);
            EXPECT_TRUE(std::isfinite(n));
        }
    }
}

TEST(ValueNoise2dTest, ExactAtLatticePointsMatchesHash) {
    float expected = static_cast<float>(flow_field_math::hash2d(4, -2) & 0xFFFFFFu) /
                          static_cast<float>(0xFFFFFFu) * 2.0f -
                      1.0f;
    EXPECT_FLOAT_EQ(flow_field_math::value_noise2d(4.0f, -2.0f), expected);
}

TEST(RandomCanvasPositionTest, StaysWithinCanvasBounds) {
    std::mt19937 rng(42);
    for (int i = 0; i < 200; ++i) {
        Vector2 pos = flow_field_math::random_canvas_position(1280.0f, 720.0f, rng);
        EXPECT_GE(pos.x, 0.0f);
        EXPECT_LE(pos.x, 1280.0f);
        EXPECT_GE(pos.y, 0.0f);
        EXPECT_LE(pos.y, 720.0f);
    }
}

TEST(RandomCanvasPositionTest, IsNotClusteredNearCenter) {
    std::mt19937 rng(1234);
    const float width = 1280.0f;
    const float height = 720.0f;
    const Vector2 center{width * 0.5f, height * 0.5f};
    const float near_center_radius = 100.0f;
    const int sample_count = 200;

    int near_center_count = 0;
    for (int i = 0; i < sample_count; ++i) {
        Vector2 pos = flow_field_math::random_canvas_position(width, height, rng);
        Vector2 delta{pos.x - center.x, pos.y - center.y};
        if (length(delta) <= near_center_radius) {
            ++near_center_count;
        }
    }

    EXPECT_LT(near_center_count, sample_count / 2)
        << "spawn positions should spread across the canvas, not cluster near center";
}

TEST(RandomCanvasPositionTest, SpansFarCornersOfCanvas) {
    std::mt19937 rng(99);
    const float width = 1280.0f;
    const float height = 720.0f;

    bool saw_left_quarter = false;
    bool saw_right_quarter = false;
    bool saw_top_quarter = false;
    bool saw_bottom_quarter = false;
    for (int i = 0; i < 200; ++i) {
        Vector2 pos = flow_field_math::random_canvas_position(width, height, rng);
        saw_left_quarter |= pos.x < width * 0.25f;
        saw_right_quarter |= pos.x > width * 0.75f;
        saw_top_quarter |= pos.y < height * 0.25f;
        saw_bottom_quarter |= pos.y > height * 0.75f;
    }

    EXPECT_TRUE(saw_left_quarter);
    EXPECT_TRUE(saw_right_quarter);
    EXPECT_TRUE(saw_top_quarter);
    EXPECT_TRUE(saw_bottom_quarter);
}

TEST(FlowFieldClampSpawnRateTest, ClampsToFlowFieldRange) {
    EXPECT_FLOAT_EQ(flow_field_math::clamp_spawn_rate_per_frame(-1.0f),
                    visual_config::kFlowFieldSpawnRatePerFrameMin);
    EXPECT_FLOAT_EQ(flow_field_math::clamp_spawn_rate_per_frame(20.0f),
                    visual_config::kFlowFieldSpawnRatePerFrameMax);
    EXPECT_FLOAT_EQ(flow_field_math::clamp_spawn_rate_per_frame(1.25f), 1.25f);
}

TEST(FlowFieldClampTurbulenceTest, ClampsToConfiguredRange) {
    EXPECT_FLOAT_EQ(flow_field_math::clamp_turbulence_frequency(0.0f),
                    visual_config::kFlowFieldTurbulenceFrequencyMin);
    EXPECT_FLOAT_EQ(flow_field_math::clamp_turbulence_frequency(10.0f),
                    visual_config::kFlowFieldTurbulenceFrequencyMax);
    EXPECT_FLOAT_EQ(flow_field_math::clamp_turbulence_frequency(0.02f), 0.02f);
}

TEST(FlowFieldClampEvolutionTest, ClampsToConfiguredRange) {
    EXPECT_FLOAT_EQ(flow_field_math::clamp_evolution_per_frame(-1.0f),
                    visual_config::kFlowFieldEvolutionPerFrameMin);
    EXPECT_FLOAT_EQ(flow_field_math::clamp_evolution_per_frame(10.0f),
                    visual_config::kFlowFieldEvolutionPerFrameMax);
    EXPECT_FLOAT_EQ(flow_field_math::clamp_evolution_per_frame(0.03f), 0.03f);
}

TEST(FlowFieldGridColsTest, ClampsToConfiguredRange) {
    EXPECT_EQ(flow_field_math::clamp_grid_cols(-100), visual_config::kFlowFieldGridColsMin);
    EXPECT_EQ(flow_field_math::clamp_grid_cols(100000), visual_config::kFlowFieldGridColsMax);
    EXPECT_EQ(flow_field_math::clamp_grid_cols(20), 20);
}

TEST(FlowFieldGridColsTest, ExtremeAndNonFiniteNumbersClampWithoutReachingTheIntCast) {
    EXPECT_EQ(flow_field_math::clamp_grid_cols_from_number(1e30), visual_config::kFlowFieldGridColsMax);
    EXPECT_EQ(flow_field_math::clamp_grid_cols_from_number(-1e30), visual_config::kFlowFieldGridColsMin);
    EXPECT_EQ(flow_field_math::clamp_grid_cols_from_number(std::numeric_limits<double>::infinity()),
              visual_config::kFlowFieldGridColsMax);
    EXPECT_EQ(flow_field_math::clamp_grid_cols_from_number(std::numeric_limits<double>::quiet_NaN()),
              visual_config::kFlowFieldGridColsMin);
    EXPECT_EQ(flow_field_math::clamp_grid_cols_from_number(20.7), 20);
}

TEST(FlowFieldGridColsTest, DefaultColsDeriveDefaultRows) {
    EXPECT_EQ(flow_field_math::grid_rows_for_cols(visual_config::kFlowFieldGridCols),
              visual_config::kFlowFieldGridRows);
}

TEST(FlowFieldGridColsTest, EveryAllowedColsValueStaysWithinTheFixedGridArray) {
    for (int cols = visual_config::kFlowFieldGridColsMin; cols <= visual_config::kFlowFieldGridColsMax; ++cols) {
        const int rows = flow_field_math::grid_rows_for_cols(cols);
        EXPECT_GE(rows, 1);
        EXPECT_LE(cols * rows, visual_config::kFlowFieldMaxGridCells);
    }
}

TEST(FieldDirectionTest, ReturnsUnitVector) {
    Vector2 dir = flow_field_math::field_direction(123.4f, 56.7f, 0.02f, 3.1f);
    EXPECT_NEAR(length(dir), 1.0f, 1e-5f);
}

TEST(FieldDirectionTest, IsDeterministic) {
    Vector2 a = flow_field_math::field_direction(10.0f, 20.0f, 0.05f, 1.0f);
    Vector2 b = flow_field_math::field_direction(10.0f, 20.0f, 0.05f, 1.0f);
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
}

TEST(FieldDirectionTest, TimePhaseChangesDirection) {
    Vector2 a = flow_field_math::field_direction(10.0f, 20.0f, 0.05f, 0.0f);
    Vector2 b = flow_field_math::field_direction(10.0f, 20.0f, 0.05f, 50.0f);
    EXPECT_FALSE(a.x == b.x && a.y == b.y);
}

TEST(GridCellIndexTest, MapsOriginToCellZero) {
    EXPECT_EQ(flow_field_math::grid_cell_index(0.0f, 0.0f, 40.0f, 40.0f, 32, 18), 0u);
}

TEST(GridCellIndexTest, MapsSecondColumnCorrectly) {
    EXPECT_EQ(flow_field_math::grid_cell_index(41.0f, 0.0f, 40.0f, 40.0f, 32, 18), 1u);
}

TEST(GridCellIndexTest, MapsSecondRowCorrectly) {
    EXPECT_EQ(flow_field_math::grid_cell_index(0.0f, 41.0f, 40.0f, 40.0f, 32, 18), 32u);
}

TEST(GridCellIndexTest, ClampsNegativeCoordinatesToFirstCell) {
    EXPECT_EQ(flow_field_math::grid_cell_index(-100.0f, -100.0f, 40.0f, 40.0f, 32, 18), 0u);
}

TEST(GridCellIndexTest, ClampsOutOfBoundsCoordinatesToLastCell) {
    std::size_t expected = static_cast<std::size_t>(17) * 32 + 31;
    EXPECT_EQ(flow_field_math::grid_cell_index(100000.0f, 100000.0f, 40.0f, 40.0f, 32, 18), expected);
}

TEST(SteerVelocityTest, FullSteerFactorSnapsToTargetDirection) {
    Vector2 current{1.0f, 0.0f};
    Vector2 target{0.0f, 1.0f};
    Vector2 result = flow_field_math::steer_velocity(current, target, 5.0f, 1.0f);
    EXPECT_NEAR(result.x, 0.0f, 1e-4f);
    EXPECT_NEAR(result.y, 5.0f, 1e-4f);
}

TEST(SteerVelocityTest, ZeroSteerFactorKeepsCurrentDirection) {
    Vector2 current{2.0f, 0.0f};
    Vector2 target{0.0f, 1.0f};
    Vector2 result = flow_field_math::steer_velocity(current, target, 5.0f, 0.0f);
    EXPECT_NEAR(result.x, 5.0f, 1e-4f);
    EXPECT_NEAR(result.y, 0.0f, 1e-4f);
}

TEST(SteerVelocityTest, ResultAlwaysMatchesRequestedSpeed) {
    Vector2 current{3.0f, 4.0f};
    Vector2 target{-1.0f, 0.0f};
    Vector2 result = flow_field_math::steer_velocity(current, target, 7.0f, 0.4f);
    EXPECT_NEAR(length(result), 7.0f, 1e-4f);
}

TEST(SteerVelocityTest, ZeroCurrentVelocityFallsBackToTargetDirection) {
    Vector2 current{0.0f, 0.0f};
    Vector2 target{0.0f, -1.0f};
    Vector2 result = flow_field_math::steer_velocity(current, target, 3.0f, 0.1f);
    EXPECT_NEAR(result.x, 0.0f, 1e-4f);
    EXPECT_NEAR(result.y, -3.0f, 1e-4f);
}

TEST(RippleInfluenceTest, PeaksAtRingRadius) {
    EXPECT_FLOAT_EQ(flow_field_math::ripple_influence(50.0f, 50.0f, 10.0f), 1.0f);
}

TEST(RippleInfluenceTest, FallsOffLinearlyWithinBandwidth) {
    EXPECT_FLOAT_EQ(flow_field_math::ripple_influence(55.0f, 50.0f, 10.0f), 0.5f);
    EXPECT_FLOAT_EQ(flow_field_math::ripple_influence(45.0f, 50.0f, 10.0f), 0.5f);
}

TEST(RippleInfluenceTest, ZeroOutsideBandwidth) {
    EXPECT_FLOAT_EQ(flow_field_math::ripple_influence(100.0f, 50.0f, 10.0f), 0.0f);
}

TEST(RippleInfluenceTest, NeverNegativeOrNan) {
    float value = flow_field_math::ripple_influence(1000.0f, 50.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(value));
    EXPECT_GE(value, 0.0f);
}

TEST(RippleImpulseTest, PointsAwayFromOriginAtPeakStrength) {
    Vector2 impulse = flow_field_math::ripple_impulse(Vector2{60.0f, 0.0f}, Vector2{0.0f, 0.0f}, 60.0f,
                                                        10.0f, 4.0f);
    EXPECT_NEAR(impulse.x, 4.0f, 1e-4f);
    EXPECT_NEAR(impulse.y, 0.0f, 1e-4f);
}

TEST(RippleImpulseTest, ZeroWhenParticleExactlyAtOrigin) {
    Vector2 impulse = flow_field_math::ripple_impulse(Vector2{0.0f, 0.0f}, Vector2{0.0f, 0.0f}, 60.0f,
                                                        10.0f, 4.0f);
    EXPECT_FLOAT_EQ(impulse.x, 0.0f);
    EXPECT_FLOAT_EQ(impulse.y, 0.0f);
}

TEST(RippleImpulseTest, ZeroFarOutsideRing) {
    Vector2 impulse = flow_field_math::ripple_impulse(Vector2{1000.0f, 0.0f}, Vector2{0.0f, 0.0f}, 60.0f,
                                                        10.0f, 4.0f);
    EXPECT_FLOAT_EQ(impulse.x, 0.0f);
    EXPECT_FLOAT_EQ(impulse.y, 0.0f);
}
