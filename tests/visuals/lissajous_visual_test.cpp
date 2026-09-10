#include "visuals/lissajous/lissajous_visual.h"

#include <cmath>

#include <gtest/gtest.h>

TEST(CompressSampleTest, PreservesEndpointsAtMagnitudeOne) {
    EXPECT_FLOAT_EQ(lissajous_math::compress_sample(1.0f, 0.5f), 1.0f);
    EXPECT_FLOAT_EQ(lissajous_math::compress_sample(-1.0f, 0.5f), -1.0f);
}

TEST(CompressSampleTest, BoostsQuietSamplesProportionallyMoreThanLoudOnes) {
    float quiet = lissajous_math::compress_sample(0.25f, 0.5f);
    float loud = lissajous_math::compress_sample(1.0f, 0.5f);

    EXPECT_FLOAT_EQ(quiet, 0.5f);
    EXPECT_FLOAT_EQ(loud, 1.0f);
    EXPECT_GT(quiet / 0.25f, loud / 1.0f);
}

TEST(CompressSampleTest, PreservesSignForNegativeInput) {
    float result = lissajous_math::compress_sample(-0.25f, 0.5f);
    EXPECT_FLOAT_EQ(result, -0.5f);
}

TEST(CompressSampleTest, ZeroStaysZeroWithNoNanOrInf) {
    float result = lissajous_math::compress_sample(0.0f, 0.5f);
    EXPECT_TRUE(std::isfinite(result));
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(CompressSampleTest, ExponentOfOneIsIdentity) {
    EXPECT_FLOAT_EQ(lissajous_math::compress_sample(0.37f, 1.0f), 0.37f);
    EXPECT_FLOAT_EQ(lissajous_math::compress_sample(-0.6f, 1.0f), -0.6f);
}

TEST(LissajousToPointsTest, AppliesCompressionCurveToRawSampleMagnitude) {
    float left[1] = {0.25f};
    float right[1] = {0.25f};
    auto pts = lissajous_math::to_points(left, right, 1, 100, 200, false);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].x, 50.0f + 0.5f * 50.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 100.0f - 0.5f * 100.0f);
}

TEST(LissajousToPointsTest, IdleReturnsCenterPointTwice) {
    auto pts = lissajous_math::to_points(nullptr, nullptr, 0, 200, 100, true);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].x, 100.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].x, 100.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}

TEST(LissajousToPointsTest, ZeroCountReturnsCenterPointEvenWhenNotIdle) {
    auto pts = lissajous_math::to_points(nullptr, nullptr, 0, 200, 100, false);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].x, 100.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
}

TEST(LissajousToPointsTest, MapsStereoPairToXY) {
    float left[2] = {1.0f, -1.0f};
    float right[2] = {1.0f, -1.0f};
    auto pts = lissajous_math::to_points(left, right, 2, 100, 200, false);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].x, 100.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 0.0f);
    EXPECT_FLOAT_EQ(pts[1].x, 0.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 200.0f);
}

TEST(LissajousToPointsTest, AmplitudeScaleShrinksOffsetFromCenter) {
    float left[1] = {1.0f};
    float right[1] = {1.0f};
    auto pts = lissajous_math::to_points(left, right, 1, 100, 200, false, 0.5f);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].x, 75.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
}

TEST(LissajousToPointsTest, DefaultAmplitudeScaleIsUnity) {
    float left[1] = {1.0f};
    float right[1] = {-1.0f};
    auto pts = lissajous_math::to_points(left, right, 1, 100, 200, false);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].x, 100.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 200.0f);
}

TEST(LissajousToPointsTest, IdleIgnoresSamplesEvenIfProvided) {
    float left[2] = {1.0f, -1.0f};
    float right[2] = {1.0f, -1.0f};
    auto pts = lissajous_math::to_points(left, right, 2, 200, 100, true);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].x, 100.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
}
