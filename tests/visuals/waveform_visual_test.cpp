#include "visuals/waveform/waveform_visual.h"

#include <gtest/gtest.h>

TEST(WaveformToPointsTest, IdleReturnsFlatMidLine) {
    auto pts = waveform_math::to_points(nullptr, 0, 200, 100, /*idle=*/true);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].x, 0.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].x, 200.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}

TEST(WaveformToPointsTest, ZeroCountReturnsFlatMidLineEvenWhenNotIdle) {
    auto pts = waveform_math::to_points(nullptr, 0, 200, 100, /*idle=*/false);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}

TEST(WaveformToPointsTest, MapsAmplitudeToYOffset) {
    float samples[3] = {1.0f, -1.0f, 0.0f};
    auto pts = waveform_math::to_points(samples, 3, 100, 200, /*idle=*/false);

    ASSERT_EQ(pts.size(), 3u);
    EXPECT_FLOAT_EQ(pts[0].y, 0.0f);    // +1.0 -> top of window
    EXPECT_FLOAT_EQ(pts[1].y, 200.0f);  // -1.0 -> bottom of window
    EXPECT_FLOAT_EQ(pts[2].y, 100.0f);  // 0.0 -> mid-line
}

TEST(WaveformToPointsTest, XSpacingCoversFullWidth) {
    float samples[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    auto pts = waveform_math::to_points(samples, 5, 400, 100, /*idle=*/false);

    ASSERT_EQ(pts.size(), 5u);
    EXPECT_FLOAT_EQ(pts.front().x, 0.0f);
    EXPECT_FLOAT_EQ(pts.back().x, 400.0f);
}

TEST(WaveformToPointsTest, SingleSampleDoesNotDivideByZero) {
    float samples[1] = {0.5f};
    auto pts = waveform_math::to_points(samples, 1, 100, 100, /*idle=*/false);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].x, 0.0f);
}

TEST(WaveformToPointsTest, IdleIgnoresSamplesEvenIfProvided) {
    float samples[3] = {1.0f, -1.0f, 1.0f};
    auto pts = waveform_math::to_points(samples, 3, 200, 100, /*idle=*/true);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}
