#include "visuals/fractal/fractal_visual.h"

#include <cmath>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

namespace {
constexpr float kEpsilon = 1e-4f;
}

TEST(FractalSelectedBandAmplitudeTest, UsesGlobalBandWhenNoFreqBandSet) {
    std::array<float, audio_config::kTotalBandSlots> energy{};
    std::array<float, audio_config::kTotalBandSlots> reference{};
    energy[audio_config::kGlobalBandIndex] = 50.0f;
    reference[audio_config::kGlobalBandIndex] = 100.0f;

    EXPECT_NEAR(fractal_math::selected_band_amplitude(energy, reference, std::nullopt), 0.5f,
                kEpsilon);
}

TEST(FractalSelectedBandAmplitudeTest, UsesSpecificBandWhenSet) {
    std::array<float, audio_config::kTotalBandSlots> energy{};
    std::array<float, audio_config::kTotalBandSlots> reference{};
    energy[3] = 25.0f;
    reference[3] = 50.0f;
    energy[audio_config::kGlobalBandIndex] = 999.0f;
    reference[audio_config::kGlobalBandIndex] = 1.0f;

    EXPECT_NEAR(fractal_math::selected_band_amplitude(energy, reference, 3), 0.5f, kEpsilon);
}

TEST(FractalSelectedBandAmplitudeTest, ClampsAboveOneWhenEnergyExceedsReference) {
    std::array<float, audio_config::kTotalBandSlots> energy{};
    std::array<float, audio_config::kTotalBandSlots> reference{};
    energy[0] = 500.0f;
    reference[0] = 100.0f;

    EXPECT_FLOAT_EQ(fractal_math::selected_band_amplitude(energy, reference, 0), 1.0f);
}

TEST(FractalSelectedBandAmplitudeTest, NonPositiveReferenceYieldsZero) {
    std::array<float, audio_config::kTotalBandSlots> energy{};
    std::array<float, audio_config::kTotalBandSlots> reference{};
    energy[0] = 10.0f;
    reference[0] = 0.0f;

    EXPECT_FLOAT_EQ(fractal_math::selected_band_amplitude(energy, reference, 0), 0.0f);
}

TEST(FractalOnePoleSmoothTest, ZeroTimeConstantSnapsInstantlyToTarget) {
    EXPECT_FLOAT_EQ(fractal_math::one_pole_smooth(0.0f, 1.0f, 0.0f, 0.016f), 1.0f);
}

TEST(FractalOnePoleSmoothTest, ZeroDtLeavesValueUnchanged) {
    EXPECT_NEAR(fractal_math::one_pole_smooth(0.3f, 1.0f, 2000.0f, 0.0f), 0.3f, kEpsilon);
}

TEST(FractalOnePoleSmoothTest, MovesOnlyASmallStepTowardTargetAfterOneFrameAtSlowTau) {
    const float result = fractal_math::one_pole_smooth(0.0f, 1.0f, 2000.0f, 0.016f);
    EXPECT_GT(result, 0.0f);
    EXPECT_LT(result, 0.05f);
}

TEST(FractalOnePoleSmoothTest, ConvergesToTargetOverManySteps) {
    float value = 0.0f;
    for (int i = 0; i < 1000; ++i) {
        value = fractal_math::one_pole_smooth(value, 1.0f, 2000.0f, 0.016f);
    }
    EXPECT_NEAR(value, 1.0f, 0.05f);
}

TEST(FractalOnePoleSmoothTest, LargerTimeConstantMovesSlowerForSameStep) {
    const float fast = fractal_math::one_pole_smooth(0.0f, 1.0f, 250.0f, 0.016f);
    const float slow = fractal_math::one_pole_smooth(0.0f, 1.0f, 2000.0f, 0.016f);
    EXPECT_LT(slow, fast);
}

TEST(FractalAmplitudeToZoomTest, ZeroAmplitudeYieldsBaseZoom) {
    EXPECT_FLOAT_EQ(fractal_math::amplitude_to_zoom(0.0f, 2.0f, 1.5f), 2.0f);
}

TEST(FractalAmplitudeToZoomTest, FullAmplitudeYieldsBaseTimesOnePlusScale) {
    EXPECT_FLOAT_EQ(fractal_math::amplitude_to_zoom(1.0f, 2.0f, 1.5f), 5.0f);
}

TEST(FractalAmplitudeToZoomTest, ClampsAmplitudeOutsideUnitRange) {
    EXPECT_FLOAT_EQ(fractal_math::amplitude_to_zoom(5.0f, 1.0f, 1.0f),
                    fractal_math::amplitude_to_zoom(1.0f, 1.0f, 1.0f));
    EXPECT_FLOAT_EQ(fractal_math::amplitude_to_zoom(-5.0f, 1.0f, 1.0f),
                    fractal_math::amplitude_to_zoom(0.0f, 1.0f, 1.0f));
}

TEST(FractalAmplitudeToZoomTest, IsMonotonicIncreasingInAmplitude) {
    const float low = fractal_math::amplitude_to_zoom(0.2f, 1.0f, 2.0f);
    const float high = fractal_math::amplitude_to_zoom(0.8f, 1.0f, 2.0f);
    EXPECT_LT(low, high);
}

TEST(FractalApplySensitivityTest, UnitSensitivityLeavesAmplitudeUnchanged) {
    EXPECT_FLOAT_EQ(fractal_math::apply_sensitivity(0.4f, 1.0f), 0.4f);
}

TEST(FractalApplySensitivityTest, ZeroSensitivityYieldsZeroRegardlessOfAmplitude) {
    EXPECT_FLOAT_EQ(fractal_math::apply_sensitivity(1.0f, 0.0f), 0.0f);
}

TEST(FractalApplySensitivityTest, ScalesAmplitudeDownAndUp) {
    EXPECT_NEAR(fractal_math::apply_sensitivity(0.4f, 0.5f), 0.2f, kEpsilon);
    EXPECT_NEAR(fractal_math::apply_sensitivity(0.2f, 2.0f), 0.4f, kEpsilon);
}

TEST(FractalApplySensitivityTest, ClampsToOneWhenScaledAmplitudeExceedsFullScale) {
    EXPECT_FLOAT_EQ(fractal_math::apply_sensitivity(0.5f, 4.0f), 1.0f);
}

TEST(FractalApplySensitivityTest, HigherSensitivityProducesLargerZoomForSameAmplitude) {
    const float low = fractal_math::amplitude_to_zoom(fractal_math::apply_sensitivity(0.3f, 0.5f), 1.0f, 0.5f);
    const float high = fractal_math::amplitude_to_zoom(fractal_math::apply_sensitivity(0.3f, 2.0f), 1.0f, 0.5f);
    EXPECT_LT(low, high);
}

TEST(FractalOrbitPhaseTest, AdvancesBySpeedTimesDtWhenClockwise) {
    EXPECT_NEAR(fractal_math::advance_orbit_phase(1.0f, 0.5f, true, 0.1f), 1.05f, kEpsilon);
}

TEST(FractalOrbitPhaseTest, CounterClockwiseAdvancesInTheOppositeDirection) {
    EXPECT_NEAR(fractal_math::advance_orbit_phase(1.0f, 0.5f, false, 0.1f), 0.95f, kEpsilon);
}

TEST(FractalOrbitPhaseTest, ZeroSpeedHoldsPhase) {
    EXPECT_FLOAT_EQ(fractal_math::advance_orbit_phase(1.25f, 0.0f, true, 0.016f), 1.25f);
}

TEST(FractalOrbitPhaseTest, ChangingSpeedMidSessionDoesNotRescaleAccumulatedPhase) {
    const float dt = 0.016f;
    const float slow_speed = 0.15f;
    const float fast_speed = 1.0f;
    const int frames_before_change = 600;

    float phase = 0.0f;
    for (int i = 0; i < frames_before_change; ++i) {
        phase = fractal_math::advance_orbit_phase(phase, slow_speed, true, dt);
    }
    const float phase_before_change = phase;
    EXPECT_NEAR(phase_before_change, slow_speed * dt * static_cast<float>(frames_before_change), 1e-3f);

    const float phase_after_change = fractal_math::advance_orbit_phase(phase, fast_speed, true, dt);

    EXPECT_NEAR(phase_after_change - phase_before_change, fast_speed * dt, kEpsilon);
    const float rescaled_total = fast_speed * dt * static_cast<float>(frames_before_change + 1);
    EXPECT_GT(std::fabs(rescaled_total - phase_after_change), 1.0f);
}

TEST(FractalOrbitPhaseTest, ReversingDirectionContinuesFromCurrentPhaseInsteadOfMirroringIt) {
    const float dt = 0.016f;
    float phase = 0.0f;
    for (int i = 0; i < 300; ++i) {
        phase = fractal_math::advance_orbit_phase(phase, 0.5f, true, dt);
    }
    const float reversed = fractal_math::advance_orbit_phase(phase, 0.5f, false, dt);
    EXPECT_NEAR(reversed, phase - 0.5f * dt, kEpsilon);
}

TEST(FractalOrbitPhaseTest, StaysWithinOneFullTurnAndPreservesAngleWhenWrapping) {
    const float unwrapped = 6.2f + 1.0f * 0.5f;
    const float wrapped = fractal_math::advance_orbit_phase(6.2f, 1.0f, true, 0.5f);
    EXPECT_LT(std::fabs(wrapped), 2.0f * PI);
    EXPECT_NEAR(std::cos(wrapped), std::cos(unwrapped), kEpsilon);
    EXPECT_NEAR(std::sin(wrapped), std::sin(unwrapped), kEpsilon);
}

TEST(FractalJuliaConstantTest, MagnitudeStaysWithinBaseRadiusPlusAmplitudeScaleAtDefaultCenter) {
    for (float phase = 0.0f; phase < 2.0f * PI; phase += 0.37f) {
        for (float amplitude : {0.0f, 0.5f, 1.0f}) {
            Vector2 c = fractal_math::julia_constant(phase, amplitude, 0.7f, 0.1f, Vector2{0.0f, 0.0f});
            const float magnitude = std::sqrt(c.x * c.x + c.y * c.y);
            EXPECT_LE(magnitude, 0.7f + 0.1f + kEpsilon);
        }
    }
}

TEST(FractalJuliaConstantTest, ZeroPhaseAndZeroAmplitudeLiesOnPositiveXAxisAtBaseRadiusFromCenter) {
    Vector2 c = fractal_math::julia_constant(0.0f, 0.0f, 0.7885f, 0.05f, Vector2{0.0f, 0.0f});
    EXPECT_NEAR(c.x, 0.7885f, kEpsilon);
    EXPECT_NEAR(c.y, 0.0f, kEpsilon);
}

TEST(FractalJuliaConstantTest, HigherAmplitudeIncreasesRadiusAtFixedPhase) {
    Vector2 low = fractal_math::julia_constant(0.15f, 0.0f, 0.7f, 0.2f, Vector2{0.0f, 0.0f});
    Vector2 high = fractal_math::julia_constant(0.15f, 1.0f, 0.7f, 0.2f, Vector2{0.0f, 0.0f});
    const float low_mag = std::sqrt(low.x * low.x + low.y * low.y);
    const float high_mag = std::sqrt(high.x * high.x + high.y * high.y);
    EXPECT_LT(low_mag, high_mag);
}

TEST(FractalJuliaConstantTest, ArbitraryOrbitCenterOffsetsResultRatherThanDefaultingToOrigin) {
    Vector2 center{2.0f, -1.0f};
    Vector2 c = fractal_math::julia_constant(0.0f, 0.0f, 0.7885f, 0.05f, center);
    EXPECT_NEAR(c.x, center.x + 0.7885f, kEpsilon);
    EXPECT_NEAR(c.y, center.y, kEpsilon);
}

TEST(FractalJuliaConstantTest, ArbitraryOrbitCenterTracksAroundTheOrbitNotJustAtZeroPhase) {
    Vector2 center{3.5f, 4.5f};
    const float phase = 0.8f;
    Vector2 c = fractal_math::julia_constant(phase, 0.3f, 0.5f, 0.1f, center);
    const float radius = 0.5f + 0.3f * 0.1f;
    EXPECT_NEAR(c.x, center.x + radius * std::cos(phase), kEpsilon);
    EXPECT_NEAR(c.y, center.y + radius * std::sin(phase), kEpsilon);
}

TEST(FractalHuePhaseTest, AdvancesBySpeedTimesDt) {
    EXPECT_NEAR(fractal_math::advance_hue_phase(0.25f, 0.05f, 2.0f), 0.35f, kEpsilon);
}

TEST(FractalHuePhaseTest, ZeroSpeedHoldsPhase) {
    EXPECT_FLOAT_EQ(fractal_math::advance_hue_phase(0.6f, 0.0f, 0.016f), 0.6f);
}

TEST(FractalHuePhaseTest, WrapsBackIntoUnitRange) {
    EXPECT_NEAR(fractal_math::advance_hue_phase(0.99f, 0.05f, 1.0f), 0.04f, kEpsilon);
}

TEST(FractalHuePhaseTest, StaysWithinUnitRangeOverManyFrames) {
    float phase = 0.0f;
    for (int i = 0; i < 20000; ++i) {
        phase = fractal_math::advance_hue_phase(phase, 0.5f, 0.016f);
        ASSERT_GE(phase, 0.0f);
        ASSERT_LT(phase, 1.0f);
    }
}

TEST(FractalHuePhaseTest, ChangingSpeedMidSessionDoesNotRescaleAccumulatedPhase) {
    const float dt = 0.016f;
    const float slow_speed = 0.01f;
    const float fast_speed = 0.5f;
    const int frames_before_change = 600;

    float phase = 0.0f;
    for (int i = 0; i < frames_before_change; ++i) {
        phase = fractal_math::advance_hue_phase(phase, slow_speed, dt);
    }
    const float phase_before_change = phase;
    EXPECT_NEAR(phase_before_change, slow_speed * dt * static_cast<float>(frames_before_change), 1e-3f);

    const float phase_after_change = fractal_math::advance_hue_phase(phase, fast_speed, dt);

    EXPECT_NEAR(phase_after_change - phase_before_change, fast_speed * dt, kEpsilon);
    const float rescaled_raw = fast_speed * dt * static_cast<float>(frames_before_change + 1);
    const float rescaled_total = rescaled_raw - std::floor(rescaled_raw);
    EXPECT_GT(std::fabs(rescaled_total - phase_after_change), 0.1f);
}

namespace {

struct FractalClampCase {
    const char* name;
    float (*clamp)(double);
    float min;
    float max;
};

const FractalClampCase kFractalClampCases[] = {
    {"base_zoom", fractal_math::clamp_base_zoom, visual_config::kFractalBaseZoomMin,
     visual_config::kFractalBaseZoomMax},
    {"zoom_amplitude_scale", fractal_math::clamp_zoom_amplitude_scale,
     visual_config::kFractalZoomAmplitudeScaleMin, visual_config::kFractalZoomAmplitudeScaleMax},
    {"julia_base_radius", fractal_math::clamp_julia_base_radius, visual_config::kFractalJuliaBaseRadiusMin,
     visual_config::kFractalJuliaBaseRadiusMax},
    {"julia_radius_amplitude_scale", fractal_math::clamp_julia_radius_amplitude_scale,
     visual_config::kFractalJuliaRadiusAmplitudeScaleMin, visual_config::kFractalJuliaRadiusAmplitudeScaleMax},
    {"julia_angular_speed", fractal_math::clamp_julia_angular_speed,
     visual_config::kFractalJuliaAngularSpeedRadiansPerSecMin,
     visual_config::kFractalJuliaAngularSpeedRadiansPerSecMax},
    {"julia_orbit_center", fractal_math::clamp_julia_orbit_center, visual_config::kFractalJuliaOrbitCenterMin,
     visual_config::kFractalJuliaOrbitCenterMax},
    {"color_cycle_speed_hz", fractal_math::clamp_color_cycle_speed_hz,
     visual_config::kFractalColorCycleSpeedHzMin, visual_config::kFractalColorCycleSpeedHzMax},
};

}

TEST(FractalClampTest, ValueInsideRangePassesThroughUnchanged) {
    for (const FractalClampCase& c : kFractalClampCases) {
        const float midpoint = (c.min + c.max) * 0.5f;
        EXPECT_FLOAT_EQ(c.clamp(static_cast<double>(midpoint)), midpoint) << c.name;
    }
}

TEST(FractalClampTest, ValuesBelowRangeIncludingExtremeAndNonFiniteClampToMinimum) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const FractalClampCase& c : kFractalClampCases) {
        for (double low : {static_cast<double>(c.min) - 0.25, -1e300, -infinity, nan}) {
            EXPECT_FLOAT_EQ(c.clamp(low), c.min) << c.name << " input " << low;
        }
    }
}

TEST(FractalClampTest, ValuesAboveRangeIncludingExtremeAndInfiniteClampToMaximum) {
    const double infinity = std::numeric_limits<double>::infinity();
    for (const FractalClampCase& c : kFractalClampCases) {
        for (double high : {static_cast<double>(c.max) + 0.25, 1e300, infinity}) {
            EXPECT_FLOAT_EQ(c.clamp(high), c.max) << c.name << " input " << high;
        }
    }
}

TEST(FractalClampTest, BaseZoomNeverGoesBelowOne) {
    EXPECT_FLOAT_EQ(fractal_math::clamp_base_zoom(0.5), 1.0f);
    EXPECT_FLOAT_EQ(fractal_math::clamp_base_zoom(0.0), 1.0f);
    EXPECT_FLOAT_EQ(fractal_math::clamp_base_zoom(-3.0), 1.0f);
}

TEST(FractalClampTest, AmplitudeSmoothingTauClampsToRangeIncludingExtremeAndNonFiniteValues) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const float min = visual_config::kFractalAmplitudeSmoothingTauMsMin;
    const float max = visual_config::kFractalAmplitudeSmoothingTauMsMax;
    const float inside = min + (max - min) * 0.25f;

    EXPECT_FLOAT_EQ(fractal_math::clamp_amplitude_smoothing_tau_ms(static_cast<double>(inside)), inside);
    for (double low : {static_cast<double>(min) - 1.0, -1e300, -infinity, nan}) {
        EXPECT_FLOAT_EQ(fractal_math::clamp_amplitude_smoothing_tau_ms(low), min) << low;
    }
    for (double high : {static_cast<double>(max) + 1.0, 1e300, infinity}) {
        EXPECT_FLOAT_EQ(fractal_math::clamp_amplitude_smoothing_tau_ms(high), max) << high;
    }
}

TEST(FractalClampTest, BoundaryMarginClampsToRangeIncludingExtremeAndNonFiniteValues) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const float min = visual_config::kFractalBoundaryMarginPxMin;
    const float max = visual_config::kFractalBoundaryMarginPxMax;
    const float inside = min + (max - min) * 0.25f;

    EXPECT_FLOAT_EQ(fractal_math::clamp_boundary_margin_px(static_cast<double>(inside)), inside);
    for (double low : {static_cast<double>(min) - 1.0, -1e300, -infinity, nan}) {
        EXPECT_FLOAT_EQ(fractal_math::clamp_boundary_margin_px(low), min) << low;
    }
    for (double high : {static_cast<double>(max) + 1.0, 1e300, infinity}) {
        EXPECT_FLOAT_EQ(fractal_math::clamp_boundary_margin_px(high), max) << high;
    }
}
