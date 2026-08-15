#include "visuals/spectrum/spectrum_visual.h"

#include <gtest/gtest.h>

TEST(SpectrumToBarsTest, ZeroBandCountReturnsEmpty) {
    auto bars = spectrum_math::to_bars(nullptr, nullptr, 0, 100, 100, 0.2f, 1.0f, /*idle=*/false);
    EXPECT_TRUE(bars.empty());
}

TEST(SpectrumToBarsTest, IdleReturnsZeroHeightBarsAtBaseline) {
    float energy[3] = {10.0f, 10.0f, 10.0f};
    float reference[3] = {10.0f, 10.0f, 10.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 3, 300, 150, 0.2f, 1.0f, /*idle=*/true);

    ASSERT_EQ(bars.size(), 3u);
    for (const auto& bar : bars) {
        EXPECT_FLOAT_EQ(bar.height, 0.0f);
        EXPECT_FLOAT_EQ(bar.y, 150.0f);
    }
}

TEST(SpectrumToBarsTest, NormalizesEachBandAgainstItsOwnReferenceAndClamps) {
    float energy[2] = {50.0f, 200.0f};
    float reference[2] = {100.0f, 100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 2, 200, 100, 0.0f, 1.0f, /*idle=*/false);

    ASSERT_EQ(bars.size(), 2u);
    EXPECT_NEAR(bars[0].height, 50.0f, 1e-4f);   // 50/100 -> half height
    EXPECT_NEAR(bars[1].height, 100.0f, 1e-4f);  // 200/100 -> clamped to full height
}

// This is the exact bug the per-band reference replaced: equal raw energy
// but different "typical" scale per band must not read as equal loudness.
TEST(SpectrumToBarsTest, DifferentReferencesGiveDifferentNormalizationForEqualEnergy) {
    float energy[2] = {10.0f, 10.0f};
    float reference[2] = {10.0f, 100.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 2, 200, 100, 0.0f, 1.0f, /*idle=*/false);

    ASSERT_EQ(bars.size(), 2u);
    EXPECT_NEAR(bars[0].height, 100.0f, 1e-4f);  // 10/10 -> full height
    EXPECT_NEAR(bars[1].height, 10.0f, 1e-4f);   // 10/100 -> 10% height
}

TEST(SpectrumToBarsTest, BarsAreEvenlySpacedWithGap) {
    float energy[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float reference[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 4, 400, 100, 0.25f, 1.0f, /*idle=*/false);

    ASSERT_EQ(bars.size(), 4u);
    EXPECT_NEAR(bars[0].x, 10.0f, 1e-4f);
    EXPECT_NEAR(bars[0].width, 80.0f, 1e-4f);
    EXPECT_NEAR(bars[1].x, 110.0f, 1e-4f);
    EXPECT_NEAR(bars[1].width, 80.0f, 1e-4f);
    EXPECT_NEAR(bars[3].x, 310.0f, 1e-4f);
}

TEST(SpectrumToBarsTest, NegativeEnergyClampsToZeroHeight) {
    float energy[1] = {-5.0f};
    float reference[1] = {1.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f, /*idle=*/false);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_FLOAT_EQ(bars[0].height, 0.0f);
}

TEST(SpectrumToBarsTest, ZeroReferenceYieldsZeroHeightInsteadOfDividingByIt) {
    float energy[1] = {5.0f};
    float reference[1] = {0.0f};
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 1.0f, /*idle=*/false);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_FLOAT_EQ(bars[0].height, 0.0f);
}

TEST(SpectrumToBarsTest, MaxHeightFractionCapsAFullyNormalizedBar) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};  // ratio == 1.0, would be full height uncapped
    auto bars = spectrum_math::to_bars(energy, reference, 1, 100, 100, 0.0f, 0.8f,
                                        /*idle=*/false);

    ASSERT_EQ(bars.size(), 1u);
    EXPECT_NEAR(bars[0].height, 80.0f, 1e-4f);  // capped to 80% of window height
    EXPECT_NEAR(bars[0].y, 20.0f, 1e-4f);       // leaves a 20px margin at the top
}
