#include "visuals/radial/radial_visual.h"

#include <cmath>

#include <gtest/gtest.h>

namespace {
constexpr float kEpsilon = 1e-3f;

float segment_length(const radial_math::RadialSegment& s) {
    const float dx = s.outer.x - s.inner.x;
    const float dy = s.outer.y - s.inner.y;
    return std::sqrt(dx * dx + dy * dy);
}
}  // namespace

TEST(RadialToBarsTest, ZeroBandCountReturnsEmpty) {
    auto segments = radial_math::to_radial_bars(nullptr, nullptr, 0, 100, 100, 0.1f, 0.9f);
    EXPECT_TRUE(segments.empty());
}

TEST(RadialToBarsTest, ZeroEnergyCollapsesToInnerRadiusPoint) {
    float energy[3] = {0.0f, 0.0f, 0.0f};
    float reference[3] = {10.0f, 10.0f, 10.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 3, 200, 200, 0.1f, 0.9f);

    ASSERT_EQ(segments.size(), 3u);
    for (const auto& segment : segments) {
        EXPECT_NEAR(segment_length(segment), 0.0f, kEpsilon);
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
    EXPECT_NEAR(segment_length(segments[0]), 50.0f, 0.5f);   // 50/100 -> half of max_length (100)
    EXPECT_NEAR(segment_length(segments[1]), 100.0f, 0.5f);  // 200/100 -> clamped to full max_length
}

TEST(RadialToBarsTest, ZeroReferenceYieldsZeroLengthInsteadOfDividingByIt) {
    float energy[1] = {5.0f};
    float reference[1] = {0.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 100, 100, 0.1f, 0.9f);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_NEAR(segment_length(segments[0]), 0.0f, kEpsilon);
}

TEST(RadialToBarsTest, SensitivityIsUncappedBeyondFullLength) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};  // ratio -> 1.0
    auto segments =
        radial_math::to_radial_bars(energy, reference, 1, 200, 200, 0.0f, 1.0f, /*sensitivity=*/2.0f);

    ASSERT_EQ(segments.size(), 1u);
    // max_length at these params is unit_radius (100); sensitivity 2.0 on a
    // fully-normalized band should double the segment past that length.
    EXPECT_NEAR(segment_length(segments[0]), 200.0f, 0.5f);
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

TEST(RadialToBarsTest, HalfAmplitudeReachesHalfwayBetweenInnerAndOuterRadius) {
    // inner_radius_fraction=0.2, max_radius_fraction=0.8 on a 200x200
    // texture -> unit_radius=100, inner_radius=20, outer_radius=80, gap=60.
    // A band at 50% amplitude should sit exactly at inner + 0.5*gap = 50.
    float energy[1] = {50.0f};
    float reference[1] = {100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 200, 200, 0.2f, 0.8f);

    ASSERT_EQ(segments.size(), 1u);
    const Vector2 center{100.0f, 100.0f};
    const float outer_dist_from_center =
        std::sqrt(std::pow(segments[0].outer.x - center.x, 2) + std::pow(segments[0].outer.y - center.y, 2));
    EXPECT_NEAR(outer_dist_from_center, 50.0f, 0.5f);
}

TEST(RadialToBarsTest, FullAmplitudeReachesExactlyOuterRadius) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 200, 200, 0.2f, 0.8f);

    ASSERT_EQ(segments.size(), 1u);
    const Vector2 center{100.0f, 100.0f};
    const float outer_dist_from_center =
        std::sqrt(std::pow(segments[0].outer.x - center.x, 2) + std::pow(segments[0].outer.y - center.y, 2));
    EXPECT_NEAR(outer_dist_from_center, 80.0f, 0.5f);  // 0.8 * unit_radius (100)
}

TEST(RadialToBarsTest, UsesSmallerDimensionForNonSquareTextures) {
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 400, 100, 0.0f, 1.0f);

    ASSERT_EQ(segments.size(), 1u);
    // unit_radius is derived from min(width, height)/2 == 50, not width/2.
    EXPECT_NEAR(segment_length(segments[0]), 50.0f, 0.5f);
}

TEST(RadialToBarsTest, InvertedInnerOuterYieldsZeroLengthRatherThanNegative) {
    // Defensive-only case: to_radial_bars doesn't itself enforce the
    // minimum-gap guardrail (that's clamp_inner_radius_fraction/
    // clamp_max_radius_fraction's job at the point of adjustment), but it
    // must not produce an inverted/negative-length segment if ever called
    // with inner >= outer directly.
    float energy[1] = {100.0f};
    float reference[1] = {100.0f};
    auto segments = radial_math::to_radial_bars(energy, reference, 1, 200, 200, 0.9f, 0.2f);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_NEAR(segment_length(segments[0]), 0.0f, kEpsilon);
}

TEST(ClampInnerRadiusFractionTest, ClampsToZeroFloor) {
    EXPECT_FLOAT_EQ(radial_math::clamp_inner_radius_fraction(-0.5f, 0.9f), 0.0f);
}

TEST(ClampInnerRadiusFractionTest, PassesThroughValueRespectingGap) {
    EXPECT_FLOAT_EQ(radial_math::clamp_inner_radius_fraction(0.3f, 0.9f), 0.3f);
}

TEST(ClampInnerRadiusFractionTest, RejectsInnerAtOrAboveOuterMinusGap) {
    // outer=0.9, min gap=0.05 -> inner clamped to at most 0.85.
    EXPECT_NEAR(radial_math::clamp_inner_radius_fraction(0.9f, 0.9f), 0.9f - radial_math::kMinRadiusGapFraction,
                kEpsilon);
    EXPECT_NEAR(radial_math::clamp_inner_radius_fraction(1.0f, 0.9f), 0.9f - radial_math::kMinRadiusGapFraction,
                kEpsilon);
}

TEST(ClampInnerRadiusFractionTest, FloorsAtZeroWhenOuterTooSmallForGap) {
    // outer=0.02 leaves no room for a 0.05 gap above 0 -- upper bound must
    // floor at 0 rather than going negative.
    EXPECT_FLOAT_EQ(radial_math::clamp_inner_radius_fraction(0.5f, 0.02f), 0.0f);
}

TEST(ClampMaxRadiusFractionTest, ClampsToOneCeiling) {
    EXPECT_FLOAT_EQ(radial_math::clamp_max_radius_fraction(1.5f, 0.1f), 1.0f);
}

TEST(ClampMaxRadiusFractionTest, PassesThroughValueRespectingGap) {
    EXPECT_FLOAT_EQ(radial_math::clamp_max_radius_fraction(0.5f, 0.1f), 0.5f);
}

TEST(ClampMaxRadiusFractionTest, RejectsOuterAtOrBelowInnerPlusGap) {
    // inner=0.1, min gap=0.05 -> outer clamped to at least 0.15.
    EXPECT_NEAR(radial_math::clamp_max_radius_fraction(0.1f, 0.1f), 0.1f + radial_math::kMinRadiusGapFraction,
                kEpsilon);
    EXPECT_NEAR(radial_math::clamp_max_radius_fraction(0.0f, 0.1f), 0.1f + radial_math::kMinRadiusGapFraction,
                kEpsilon);
}

TEST(ClampMaxRadiusFractionTest, CapsAtOneWhenInnerTooCloseToOneForGap) {
    // inner=0.98 leaves no room for a 0.05 gap below 1 -- lower bound must
    // cap at 1 rather than exceeding it.
    EXPECT_FLOAT_EQ(radial_math::clamp_max_radius_fraction(0.5f, 0.98f), 1.0f);
}
