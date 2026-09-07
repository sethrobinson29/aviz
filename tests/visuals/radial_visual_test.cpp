#include "visuals/radial/radial_visual.h"

#include <cmath>

#include <gtest/gtest.h>

#include "visuals/visual_config.h"

namespace {
constexpr float kEpsilon = 1e-3f;
}

TEST(RadialToBarsTest, ZeroBandCountReturnsEmpty) {
    auto segments = radial_math::to_radial_bars(nullptr, nullptr, 0, 100, 100, 0.1f, 0.9f);
    EXPECT_TRUE(segments.empty());
}

TEST(RadialToBarsTest, ZeroEnergySnapsToMinBarHeightFloor) {
    float energy[3] = {0.0f, 0.0f, 0.0f};
    float reference[3] = {10.0f, 10.0f, 10.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 3, 200, 200, 0.1f, 0.9f);

    ASSERT_EQ(segments.size(), 3u);
    for (const auto& segment : segments) {
        const float dx = segment.outer.x - segment.inner.x;
        const float dy = segment.outer.y - segment.inner.y;
        EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), visual_config::kMinBarHeightPx, kEpsilon);
    }
}

TEST(RadialToBarsTest, FirstBandPointsStraightUpFromCenter) {
    float energy[4] = {100.0f, 0.0f, 0.0f, 0.0f};
    float reference[4] = {100.0f, 1.0f, 1.0f, 1.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 4, 200, 200, 0.1f, 0.9f);

    ASSERT_EQ(segments.size(), 4u);
    // Center is (100, 100); band 0 sits at -90 degrees (straight up), so its
    // segment's x stays at the center's x for both endpoints.
    EXPECT_NEAR(segments[0].inner.x, 100.0f, kEpsilon);
    EXPECT_NEAR(segments[0].outer.x, 100.0f, kEpsilon);
    EXPECT_LT(segments[0].outer.y, segments[0].inner.y);
}

TEST(RadialToBarsTest, BandsAreEvenlySpacedByAngle) {
    float energy[4] = {50.0f, 50.0f, 50.0f, 50.0f};
    float reference[4] = {100.0f, 100.0f, 100.0f, 100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 4, 200, 200, 0.1f, 0.9f);

    ASSERT_EQ(segments.size(), 4u);
    // Band 2 is 180 degrees from band 0 -- mirrored across the center on
    // both axes.
    const Vector2 center{100.0f, 100.0f};
    EXPECT_NEAR(segments[2].outer.x - center.x, -(segments[0].outer.x - center.x), kEpsilon);
    EXPECT_NEAR(segments[2].outer.y - center.y, -(segments[0].outer.y - center.y), kEpsilon);
}

TEST(RadialToBarsTest, NormalizesEachBandAgainstItsOwnReferenceAndClamps) {
    float energy[2] = {50.0f, 200.0f};
    float reference[2] = {100.0f, 100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 2, 200, 200, 0.0f, 1.0f);

    ASSERT_EQ(segments.size(), 2u);
    auto length = [](const radial_math::RadialSegment& s) {
        const float dx = s.outer.x - s.inner.x;
        const float dy = s.outer.y - s.inner.y;
        return std::sqrt(dx * dx + dy * dy);
    };
    EXPECT_NEAR(length(segments[0]), 50.0f, 0.5f);   // 50/100 -> half of max_length (100)
    EXPECT_NEAR(length(segments[1]), 100.0f, 0.5f);  // 200/100 -> clamped to full max_length
}

TEST(RadialToBarsTest, ZeroReferenceYieldsMinBarHeightFloorInsteadOfDividingByIt) {
    float energy[1] = {5.0f};
    float reference[1] = {0.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 100, 100, 0.1f, 0.9f);

    ASSERT_EQ(segments.size(), 1u);
    const float dx = segments[0].outer.x - segments[0].inner.x;
    const float dy = segments[0].outer.y - segments[0].inner.y;
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), visual_config::kMinBarHeightPx, kEpsilon);
}

TEST(RadialToBarsTest, SensitivityIsUncappedBeyondFullLength) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};  // ratio -> 1.0
    auto segments =
        radial_math::to_radial_bars(energy, reference, 1, 200, 200, 0.0f, 1.0f, /*sensitivity=*/2.0f);

    ASSERT_EQ(segments.size(), 1u);
    const float dx = segments[0].outer.x - segments[0].inner.x;
    const float dy = segments[0].outer.y - segments[0].inner.y;
    // max_length at these params is unit_radius (100); sensitivity 2.0 on a
    // fully-normalized band should double the segment past that length.
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), 200.0f, 0.5f);
}

TEST(RadialToBarsTest, InnerRadiusFractionOffsetsSegmentStart) {
    float energy[1] = {0.0f};
    float reference[1] = {1.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 200, 200, 0.25f, 0.9f);

    ASSERT_EQ(segments.size(), 1u);
    const Vector2 center{100.0f, 100.0f};
    const float dx = segments[0].inner.x - center.x;
    const float dy = segments[0].inner.y - center.y;
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), 100.0f * 0.25f, kEpsilon);
}

TEST(RadialToBarsTest, UsesSmallerDimensionForNonSquareTextures) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 400, 100, 0.0f, 1.0f);

    ASSERT_EQ(segments.size(), 1u);
    const float dx = segments[0].outer.x - segments[0].inner.x;
    const float dy = segments[0].outer.y - segments[0].inner.y;
    // unit_radius is derived from min(width, height)/2 == 50, not width/2.
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), 50.0f, 0.5f);
}
