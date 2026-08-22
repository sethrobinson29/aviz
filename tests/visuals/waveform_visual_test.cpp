#include "visuals/waveform/waveform_visual.h"

#include <gtest/gtest.h>

#include "visuals/visual_config.h"

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

TEST(WaveformToPointsTest, AmplitudeScaleShrinksOffsetFromMidline) {
    float samples[1] = {1.0f};
    auto pts = waveform_math::to_points(samples, 1, 100, 200, /*idle=*/false,
                                         /*amplitude_scale=*/0.5f);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);  // mid(100) - 1.0*0.5*100 = 50
}

TEST(WaveformToPointsTest, DefaultAmplitudeScaleIsUnity) {
    float samples[1] = {1.0f};
    auto pts = waveform_math::to_points(samples, 1, 100, 200, /*idle=*/false);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].y, 0.0f);  // same as MapsAmplitudeToYOffset's +1.0 case
}

TEST(BandAmplitudeScaleTest, NormalizesAndClamps) {
    EXPECT_FLOAT_EQ(waveform_math::band_amplitude_scale(50.0f, 100.0f), 0.5f);
    EXPECT_FLOAT_EQ(waveform_math::band_amplitude_scale(200.0f, 100.0f), 1.0f);
    EXPECT_FLOAT_EQ(waveform_math::band_amplitude_scale(-5.0f, 100.0f), 0.0f);
}

TEST(BandAmplitudeScaleTest, NonPositiveReferenceYieldsZero) {
    EXPECT_FLOAT_EQ(waveform_math::band_amplitude_scale(5.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(waveform_math::band_amplitude_scale(5.0f, -1.0f), 0.0f);
}

TEST(PulseThicknessTest, ZeroPulseReturnsBaseThicknessUnchanged) {
    EXPECT_FLOAT_EQ(waveform_math::pulse_thickness(3.0f, 0.0f), 3.0f);
}

TEST(PulseThicknessTest, FullPulseAppliesBoostFactor) {
    EXPECT_FLOAT_EQ(waveform_math::pulse_thickness(2.0f, 1.0f),
                    2.0f * visual_config::kOnsetPulseThicknessBoost);
}

TEST(PulseThicknessTest, ClampsOutOfRangePulse) {
    EXPECT_FLOAT_EQ(waveform_math::pulse_thickness(2.0f, 5.0f),
                    waveform_math::pulse_thickness(2.0f, 1.0f));
    EXPECT_FLOAT_EQ(waveform_math::pulse_thickness(2.0f, -5.0f),
                    waveform_math::pulse_thickness(2.0f, 0.0f));
}
