#include "visuals/waveform/waveform_visual.h"

#include <gtest/gtest.h>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

TEST(WaveformToPointsTest, IdleReturnsFlatMidLine) {
    auto pts = waveform_math::to_points(nullptr, 0, 200, 100, true);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].x, 0.0f);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].x, 200.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}

TEST(WaveformToPointsTest, ZeroCountReturnsFlatMidLineEvenWhenNotIdle) {
    auto pts = waveform_math::to_points(nullptr, 0, 200, 100, false);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}

TEST(WaveformToPointsTest, MapsAmplitudeToYOffset) {
    float samples[3] = {1.0f, -1.0f, 0.0f};
    auto pts = waveform_math::to_points(samples, 3, 100, 200, false);

    ASSERT_EQ(pts.size(), 3u);
    EXPECT_FLOAT_EQ(pts[0].y, 0.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 200.0f);
    EXPECT_FLOAT_EQ(pts[2].y, 100.0f);
}

TEST(WaveformToPointsTest, XSpacingCoversFullWidth) {
    float samples[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    auto pts = waveform_math::to_points(samples, 5, 400, 100, false);

    ASSERT_EQ(pts.size(), 5u);
    EXPECT_FLOAT_EQ(pts.front().x, 0.0f);
    EXPECT_FLOAT_EQ(pts.back().x, 400.0f);
}

TEST(WaveformToPointsTest, SingleSampleDoesNotDivideByZero) {
    float samples[1] = {0.5f};
    auto pts = waveform_math::to_points(samples, 1, 100, 100, false);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].x, 0.0f);
}

TEST(WaveformToPointsTest, IdleIgnoresSamplesEvenIfProvided) {
    float samples[3] = {1.0f, -1.0f, 1.0f};
    auto pts = waveform_math::to_points(samples, 3, 200, 100, true);

    ASSERT_EQ(pts.size(), 2u);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
    EXPECT_FLOAT_EQ(pts[1].y, 50.0f);
}

TEST(WaveformToPointsTest, AmplitudeScaleShrinksOffsetFromMidline) {
    float samples[1] = {1.0f};
    auto pts = waveform_math::to_points(samples, 1, 100, 200, false,
                                         0.5f);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].y, 50.0f);
}

TEST(WaveformToPointsTest, DefaultAmplitudeScaleIsUnity) {
    float samples[1] = {1.0f};
    auto pts = waveform_math::to_points(samples, 1, 100, 200, false);

    ASSERT_EQ(pts.size(), 1u);
    EXPECT_FLOAT_EQ(pts[0].y, 0.0f);
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

TEST(NextOnsetPulseTest, DecaysWhenNoOnsetAndBandUnchanged) {
    float pulse = waveform_math::next_onset_pulse(0.8f, false, false, 2.0f, 0.85f);
    EXPECT_FLOAT_EQ(pulse, 0.8f * 0.85f);
}

TEST(NextOnsetPulseTest, SnapsToTargetOnActiveOnset) {
    float pulse = waveform_math::next_onset_pulse(0.1f, false, true, 2.0f, 0.85f);
    EXPECT_FLOAT_EQ(pulse, 2.0f);
}

TEST(NextOnsetPulseTest, BandChangeDropsStaleCarryoverBeforeDecay) {
    float pulse = waveform_math::next_onset_pulse(0.9f, true, false, 2.0f, 0.85f);
    EXPECT_FLOAT_EQ(pulse, 0.0f);
}

TEST(NextOnsetPulseTest, BandChangeDoesNotSuppressAFreshOnsetOnTheNewBand) {
    float pulse = waveform_math::next_onset_pulse(0.9f, true, true, 2.0f, 0.85f);
    EXPECT_FLOAT_EQ(pulse, 2.0f);
}

TEST(ResolveReactivityTest, BandAmplitudeScalesFromItsOwnBandNotOthers) {
    AudioFeatures features;
    features.band_energy[3] = 50.0f;
    features.band_agc_reference[3] = 100.0f;
    features.band_energy[4] = 999.0f;

    LayerReactivity reactivity{3, ReactivitySource::BandAmplitude};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_FLOAT_EQ(result.amplitude_scale, 0.5f);
    EXPECT_FALSE(result.onset_active);
}

TEST(ResolveReactivityTest, BandOnsetReadsItsOwnBandNotOthers) {
    AudioFeatures features;
    features.band_onset[3] = true;
    features.band_onset[4] = false;

    LayerReactivity reactivity{3, ReactivitySource::BandOnset};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_TRUE(result.onset_active);
    EXPECT_FLOAT_EQ(result.amplitude_scale, 1.0f);
}

TEST(ResolveReactivityTest, BandOnsetFalseWhenThatBandHasNoOnset) {
    AudioFeatures features;
    features.band_onset[3] = false;

    LayerReactivity reactivity{3, ReactivitySource::BandOnset};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_FALSE(result.onset_active);
}

TEST(ResolveReactivityTest, GlobalBeatReadsTheGlobalBandNotABand) {
    AudioFeatures features;
    features.band_onset[audio_config::kGlobalBandIndex] = true;
    features.band_onset[0] = false;

    LayerReactivity reactivity{std::nullopt, ReactivitySource::GlobalBeat};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_TRUE(result.onset_active);
}

TEST(ResolveReactivityTest, BandAmplitudeIsDirectMultipliedBySensitivity) {
    AudioFeatures features;
    features.band_energy[3] = 50.0f;
    features.band_agc_reference[3] = 100.0f;

    LayerReactivity reactivity{3, ReactivitySource::BandAmplitude, 3.0f};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_FLOAT_EQ(result.amplitude_scale, 1.5f);
}

TEST(ResolveReactivityTest, BandOnsetSetsPulseTargetToSensitivity) {
    AudioFeatures features;
    features.band_onset[3] = true;

    LayerReactivity reactivity{3, ReactivitySource::BandOnset, 2.5f};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_TRUE(result.onset_active);
    EXPECT_FLOAT_EQ(result.onset_pulse_target, 2.5f);
}

TEST(ResolveReactivityTest, PulseTargetIsUnusedDefaultWhenOnsetInactive) {
    AudioFeatures features;
    features.band_onset[3] = false;

    LayerReactivity reactivity{3, ReactivitySource::BandOnset, 2.5f};
    auto result = waveform_math::resolve_reactivity(features, reactivity);

    EXPECT_FALSE(result.onset_active);
    EXPECT_FLOAT_EQ(result.onset_pulse_target, 0.0f);
}
