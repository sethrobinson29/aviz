#include "visuals/fractal/fractal_visual.h"

#include <cmath>
#include <optional>

#include <gtest/gtest.h>

#include "audio/audio_config.h"

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

TEST(FractalSelectedBandOnsetTest, ReadsGlobalBandByDefault) {
    std::array<bool, audio_config::kTotalBandSlots> onset{};
    onset[audio_config::kGlobalBandIndex] = true;

    EXPECT_TRUE(fractal_math::selected_band_onset(onset, std::nullopt));
    EXPECT_FALSE(fractal_math::selected_band_onset(onset, 0));
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

TEST(FractalJuliaConstantTest, MagnitudeStaysWithinBaseRadiusPlusAmplitudeScale) {
    for (float t = 0.0f; t < 10.0f; t += 0.37f) {
        for (float amplitude : {0.0f, 0.5f, 1.0f}) {
            Vector2 c = fractal_math::julia_constant(t, amplitude, 0.7f, 0.1f, 0.2f);
            const float magnitude = std::sqrt(c.x * c.x + c.y * c.y);
            EXPECT_LE(magnitude, 0.7f + 0.1f + kEpsilon);
        }
    }
}

TEST(FractalJuliaConstantTest, ZeroTimeAndZeroAmplitudeLiesOnPositiveXAxisAtBaseRadius) {
    Vector2 c = fractal_math::julia_constant(0.0f, 0.0f, 0.7885f, 0.05f, 0.15f);
    EXPECT_NEAR(c.x, 0.7885f, kEpsilon);
    EXPECT_NEAR(c.y, 0.0f, kEpsilon);
}

TEST(FractalJuliaConstantTest, HigherAmplitudeIncreasesRadiusAtFixedTime) {
    Vector2 low = fractal_math::julia_constant(1.0f, 0.0f, 0.7f, 0.2f, 0.15f);
    Vector2 high = fractal_math::julia_constant(1.0f, 1.0f, 0.7f, 0.2f, 0.15f);
    const float low_mag = std::sqrt(low.x * low.x + low.y * low.y);
    const float high_mag = std::sqrt(high.x * high.x + high.y * high.y);
    EXPECT_LT(low_mag, high_mag);
}

TEST(FractalColorCycleHueTest, StaysWithinUnitRange) {
    for (float t = 0.0f; t < 1000.0f; t += 37.0f) {
        const float hue = fractal_math::color_cycle_hue(t, 0.05f, 0.5f, 0.6f);
        EXPECT_GE(hue, 0.0f);
        EXPECT_LT(hue, 1.0f);
    }
}

TEST(FractalColorCycleHueTest, ZeroInputsYieldZeroHue) {
    EXPECT_NEAR(fractal_math::color_cycle_hue(0.0f, 0.05f, 0.0f, 0.6f), 0.0f, kEpsilon);
}

TEST(FractalColorCycleHueTest, PulseShiftsHueForwardBeforeWrapping) {
    const float without_pulse = fractal_math::color_cycle_hue(1.0f, 0.05f, 0.0f, 0.6f);
    const float with_pulse = fractal_math::color_cycle_hue(1.0f, 0.05f, 0.5f, 0.6f);
    EXPECT_GT(with_pulse, without_pulse);
}
