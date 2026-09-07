#include "visuals/spectrum/spectrum_visual.h"

#include <gtest/gtest.h>

#include "visuals/visual_config.h"

TEST(SpectrumToBarsTest, ZeroBandCountReturnsEmpty) {
    auto bars = spectrum_math::to_bars(nullptr, nullptr, 0, 100, 100, 0.2f, 1.0f);
    EXPECT_TRUE(bars.empty());
}

TEST(SpectrumToBarsTest, ZeroEnergySnapsToMinBarHeightFloor) {
    float energy[3] = {0.0f, 0.0f, 0.0f};
    float reference[3] = {10.0f, 10.0f, 10.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 3, 300, 150, 0.2f, 1.0f);

    ASSERT_EQ(bars.size(), 3u);
    for (const auto& bar : bars) {
        EXPECT_FLOAT_EQ(bar.height, visual_config::kMinBarHeightPx);
        EXPECT_FLOAT_EQ(bar.y, 150.0f - visual_config::kMinBarHeightPx);
    }
}

TEST(SpectrumToBarsTest, LowButRealEnergyRendersNonZeroHeightRegardlessOfOtherBands) {
    float energy[2] = {0.0f, 0.045f};
    float reference[2] = {10.0f, 0.11f};  // band 1 ratio ~0.409
    auto bars = spectrum_math::to_bars(energy, reference, 2, 200, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 2u);
    EXPECT_FLOAT_EQ(bars[0].height, visual_config::kMinBarHeightPx);
    EXPECT_NEAR(bars[1].height, 40.9f, 0.5f);
}

TEST(SpectrumToBarsTest, NormalizesEachBandAgainstItsOwnReferenceAndClamps) {
    float energy[2] = {50.0f, 200.0f};
    float reference[2] = {100.0f, 100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 2, 200, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 2u);
    EXPECT_NEAR(bars[0].height, 50.0f, 1e-4f);   // 50/100 -> half height
    EXPECT_NEAR(bars[1].height, 100.0f, 1e-4f);  // 200/100 -> clamped to full height
}

TEST(SpectrumToBarsTest, DifferentReferencesGiveDifferentNormalizationForEqualEnergy) {
    float energy[2] = {10.0f, 10.0f};
    float reference[2] = {10.0f, 100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 2, 200, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 2u);
    EXPECT_NEAR(bars[0].height, 100.0f, 1e-4f);  // 10/10 -> full height
    EXPECT_NEAR(bars[1].height, 10.0f, 1e-4f);   // 10/100 -> 10% height
}

TEST(SpectrumToBarsTest, BarsAreEvenlySpacedWithGap) {
    float energy[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float reference[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 4, 400, 100, 0.25f, 1.0f);

    ASSERT_EQ(bars.size(), 4u);
    EXPECT_NEAR(bars[0].x, 10.0f, 1e-4f);
    EXPECT_NEAR(bars[0].width, 80.0f, 1e-4f);
    EXPECT_NEAR(bars[1].x, 110.0f, 1e-4f);
    EXPECT_NEAR(bars[1].width, 80.0f, 1e-4f);
    EXPECT_NEAR(bars[3].x, 310.0f, 1e-4f);
}

TEST(SpectrumToBarsTest, NegativeEnergyClampsToMinBarHeightFloor) {
    float energy[1] = {-5.0f};
    float reference[1] = {1.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_FLOAT_EQ(bars[0].height, visual_config::kMinBarHeightPx);
}

TEST(SpectrumToBarsTest, ZeroReferenceYieldsMinBarHeightFloorInsteadOfDividingByIt) {
    float energy[1] = {5.0f};
    float reference[1] = {0.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_FLOAT_EQ(bars[0].height, visual_config::kMinBarHeightPx);
}

TEST(SpectrumToBarsTest, MinBarHeightFloorDoesNotAffectBarsAboveIt) {
    float energy[1] = {50.0f};
    float reference[1] = {100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_NEAR(bars[0].height, 50.0f, 1e-4f);
}

TEST(SpectrumToBarsTest, MaxHeightFractionCapsAFullyNormalizedBar) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 0.8f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_NEAR(bars[0].height, 80.0f, 1e-4f);  // capped to 80% of window height
    EXPECT_NEAR(bars[0].y, 20.0f, 1e-4f);       // leaves a 20px margin at the top
}

TEST(BarIndicesToDrawTest, GlobalDrawsEveryIndexInOrder) {
    auto indices = spectrum_math::bar_indices_to_draw(std::nullopt, 5);
    EXPECT_EQ(indices, (std::vector<std::size_t>{0, 1, 2, 3, 4}));
}

TEST(BarIndicesToDrawTest, GlobalWithZeroBarsIsEmpty) {
    auto indices = spectrum_math::bar_indices_to_draw(std::nullopt, 0);
    EXPECT_TRUE(indices.empty());
}

TEST(BarIndicesToDrawTest, BandScopedDrawsOnlyItsOwnIndex) {
    auto indices = spectrum_math::bar_indices_to_draw(3, 16);
    EXPECT_EQ(indices, (std::vector<std::size_t>{3}));
}

TEST(BarIndicesToDrawTest, OutOfRangeBandDrawsNothing) {
    auto indices = spectrum_math::bar_indices_to_draw(16, 16);
    EXPECT_TRUE(indices.empty());
}

TEST(SpectrumToBarsTest, DefaultSensitivityIsUnity) {
    float energy[1] = {50.0f};
    float reference[1] = {100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_NEAR(bars[0].height, 50.0f, 1e-4f);
}

TEST(SpectrumToBarsTest, SensitivityDirectMultipliesNormalizedRatio) {
    float energy[1] = {50.0f};
    float reference[1] = {100.0f};  // ratio -> 0.5
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f,
                                        /*sensitivity=*/2.0f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_NEAR(bars[0].height, 100.0f, 1e-4f);  // 0.5 * 2.0 -> full height
}

TEST(SpectrumToBarsTest, SensitivityIsUncappedBeyondFullHeight) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};  // ratio -> 1.0
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f,
                                        /*sensitivity=*/2.0f);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_NEAR(bars[0].height, 200.0f, 1e-4f);  // uncapped, exceeds window height
}

TEST(UpdateOnsetPulseTest, OnsetSnapsToSensitivity) {
    EXPECT_FLOAT_EQ(spectrum_math::update_onset_pulse(0.1f, /*onset=*/true, /*sensitivity=*/2.5f), 2.5f);
}

TEST(UpdateOnsetPulseTest, NoOnsetDecaysCurrentValue) {
    EXPECT_FLOAT_EQ(spectrum_math::update_onset_pulse(1.0f, /*onset=*/false, /*sensitivity=*/1.0f),
                    visual_config::kSpectrumOnsetPulseDecayPerFrame);
}

TEST(PulseColorTest, ZeroPulseRestsAtDimFraction) {
    Color base{200, 200, 200, 200};
    Color result = spectrum_math::pulse_color(base, 0.0f);
    const unsigned char expected = static_cast<unsigned char>(
        200.0f * visual_config::kSpectrumOnsetPulseRestBrightnessFraction);
    EXPECT_EQ(result.r, expected);
    EXPECT_EQ(result.g, expected);
    EXPECT_EQ(result.b, expected);
    EXPECT_EQ(result.a, 200);
}

TEST(PulseColorTest, FullPulseReachesBaseColorUnchanged) {
    Color base{50, 100, 150, 200};
    Color result = spectrum_math::pulse_color(base, 1.0f);
    EXPECT_EQ(result.r, 50);
    EXPECT_EQ(result.g, 100);
    EXPECT_EQ(result.b, 150);
    EXPECT_EQ(result.a, 200);
}

TEST(PulseColorTest, AlphaIsNeverModifiedRegardlessOfPulse) {
    Color base{50, 100, 150, 77};
    EXPECT_EQ(spectrum_math::pulse_color(base, 0.0f).a, 77);
    EXPECT_EQ(spectrum_math::pulse_color(base, 0.5f).a, 77);
    EXPECT_EQ(spectrum_math::pulse_color(base, 1.0f).a, 77);
}

TEST(PulseColorTest, OpaqueWhiteBaseStillDistinguishesRestFromPeak) {
    Color base = WHITE;
    Color rest = spectrum_math::pulse_color(base, 0.0f);
    Color peak = spectrum_math::pulse_color(base, 1.0f);
    EXPECT_LT(rest.r, peak.r);
}

TEST(PulseColorTest, PartialPulseLerpsFromDimFractionToBase) {
    Color base{50, 100, 150, 255};
    Color result = spectrum_math::pulse_color(base, 0.5f);
    const float kFraction = visual_config::kSpectrumOnsetPulseRestBrightnessFraction;
    auto expect_channel = [](float base_c, float fraction) {
        const float rest = base_c * fraction;
        return static_cast<unsigned char>(rest + (base_c - rest) * 0.5f);
    };
    EXPECT_EQ(result.r, expect_channel(50.0f, kFraction));
    EXPECT_EQ(result.g, expect_channel(100.0f, kFraction));
    EXPECT_EQ(result.b, expect_channel(150.0f, kFraction));
}

TEST(PulseColorTest, ClampsOutOfRangePulse) {
    Color base{50, 100, 150, 255};
    Color over = spectrum_math::pulse_color(base, 5.0f);
    Color at_max = spectrum_math::pulse_color(base, 1.0f);
    Color under = spectrum_math::pulse_color(base, -5.0f);
    Color at_min = spectrum_math::pulse_color(base, 0.0f);
    EXPECT_EQ(over.r, at_max.r);
    EXPECT_EQ(under.r, at_min.r);
}
