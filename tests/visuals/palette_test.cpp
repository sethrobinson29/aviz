#include "visuals/palette.h"

#include <gtest/gtest.h>

TEST(PaletteSampleTest, SingleStopAlwaysReturnsThatColor) {
    Palette palette{{{0.5f, Color{10, 20, 30, 255}}}};
    Color result = palette_math::sample(palette, 0.0f);
    EXPECT_EQ(result.r, 10);
    EXPECT_EQ(result.g, 20);
    EXPECT_EQ(result.b, 30);
}

TEST(PaletteSampleTest, ExactStopPositionsReturnExactColors) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{255, 255, 255, 255}}}};
    EXPECT_EQ(palette_math::sample(palette, 0.0f).r, 0);
    EXPECT_EQ(palette_math::sample(palette, 1.0f).r, 255);
}

TEST(PaletteSampleTest, InterpolatesLinearlyBetweenTwoStops) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{200, 0, 0, 255}}}};
    Color result = palette_math::sample(palette, 0.5f);
    EXPECT_NEAR(result.r, 100, 1);
}

TEST(PaletteSampleTest, ClampsBelowFirstStop) {
    Palette palette{{{0.2f, Color{50, 0, 0, 255}}, {0.8f, Color{200, 0, 0, 255}}}};
    Color result = palette_math::sample(palette, -1.0f);
    EXPECT_EQ(result.r, 50);
}

TEST(PaletteSampleTest, ClampsAboveLastStop) {
    Palette palette{{{0.2f, Color{50, 0, 0, 255}}, {0.8f, Color{200, 0, 0, 255}}}};
    Color result = palette_math::sample(palette, 5.0f);
    EXPECT_EQ(result.r, 200);
}

TEST(PaletteSampleTest, ThreeStopsInterpolatesWithinCorrectSegment) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}},
                      {0.5f, Color{100, 100, 100, 255}},
                      {1.0f, Color{200, 200, 200, 255}}}};
    Color mid_low = palette_math::sample(palette, 0.25f);
    EXPECT_NEAR(mid_low.r, 50, 1);
    Color mid_high = palette_math::sample(palette, 0.75f);
    EXPECT_NEAR(mid_high.r, 150, 1);
}

TEST(PaletteSampleTest, InterpolatesAlphaChannelToo) {
    Palette palette{{{0.0f, Color{0, 0, 0, 0}}, {1.0f, Color{0, 0, 0, 200}}}};
    Color result = palette_math::sample(palette, 0.5f);
    EXPECT_NEAR(result.a, 100, 1);
}

TEST(SampleByIndexTest, FirstAndLastIndexReachEndStops) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{200, 0, 0, 255}}}};
    EXPECT_EQ(palette_math::sample_by_index(palette, 0, 5).r, 0);
    EXPECT_EQ(palette_math::sample_by_index(palette, 4, 5).r, 200);
}

TEST(SampleByIndexTest, MiddleIndexInterpolates) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{200, 0, 0, 255}}}};
    Color result = palette_math::sample_by_index(palette, 2, 5);
    EXPECT_NEAR(result.r, 100, 1);
}

TEST(SampleByIndexTest, SingleElementCountNormalizesToZero) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{200, 0, 0, 255}}}};
    EXPECT_EQ(palette_math::sample_by_index(palette, 0, 1).r, 0);
}

TEST(PaletteEqualityTest, IdenticalStopsAreEqual) {
    Palette a{{{0.0f, Color{1, 2, 3, 4}}, {1.0f, Color{5, 6, 7, 8}}}};
    Palette b{{{0.0f, Color{1, 2, 3, 4}}, {1.0f, Color{5, 6, 7, 8}}}};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(PaletteEqualityTest, DifferentStopCountsAreUnequal) {
    Palette a{{{0.0f, Color{1, 2, 3, 4}}}};
    Palette b{{{0.0f, Color{1, 2, 3, 4}}, {1.0f, Color{5, 6, 7, 8}}}};
    EXPECT_TRUE(a != b);
}

TEST(PaletteEqualityTest, SameStopCountDifferentColorIsUnequal) {
    Palette a{{{0.0f, Color{1, 2, 3, 4}}, {1.0f, Color{5, 6, 7, 8}}}};
    Palette b{{{0.0f, Color{1, 2, 3, 4}}, {1.0f, Color{9, 6, 7, 8}}}};
    EXPECT_TRUE(a != b);
}

TEST(ToGradientPixelsTest, EndpointsMatchStopColorsExactly) {
    Palette palette{{{0.0f, Color{10, 20, 30, 255}}, {1.0f, Color{200, 210, 220, 255}}}};
    std::vector<Color> pixels = palette_math::to_gradient_pixels(palette, 16);

    ASSERT_EQ(pixels.size(), 16u);
    EXPECT_EQ(pixels.front().r, 10);
    EXPECT_EQ(pixels.back().r, 200);
}

TEST(ToGradientPixelsTest, WidthOneReturnsSinglePixelAtStart) {
    Palette palette{{{0.0f, Color{10, 20, 30, 255}}, {1.0f, Color{200, 210, 220, 255}}}};
    std::vector<Color> pixels = palette_math::to_gradient_pixels(palette, 1);

    ASSERT_EQ(pixels.size(), 1u);
    EXPECT_EQ(pixels.front().r, 10);
}

TEST(ToGradientPixelsTest, NonPositiveWidthStillReturnsOnePixel) {
    Palette palette{{{0.0f, Color{10, 20, 30, 255}}, {1.0f, Color{200, 210, 220, 255}}}};
    std::vector<Color> pixels = palette_math::to_gradient_pixels(palette, 0);

    EXPECT_EQ(pixels.size(), 1u);
}
