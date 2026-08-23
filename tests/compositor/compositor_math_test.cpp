#include "compositor/compositor_math.h"

#include <gtest/gtest.h>

#include "raylib.h"
#include "rlgl.h"

TEST(ClampOpacityTest, ClampsIntoZeroOneRange) {
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(1.5f), 1.0f);
}

TEST(ClampSensitivityTest, ClampsIntoValidRange) {
    EXPECT_FLOAT_EQ(compositor_math::clamp_sensitivity(-1.0f), compositor_math::kSensitivityMin);
    EXPECT_FLOAT_EQ(compositor_math::clamp_sensitivity(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_sensitivity(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_sensitivity(compositor_math::kSensitivityMax),
                     compositor_math::kSensitivityMax);
    EXPECT_FLOAT_EQ(compositor_math::clamp_sensitivity(compositor_math::kSensitivityMax + 1.0f),
                     compositor_math::kSensitivityMax);
}

TEST(ToBlendFactorsTest, BuiltInModesMapToRaylibConstants) {
    EXPECT_EQ(compositor_math::to_blend_factors(LayerBlendMode::Normal).raylib_mode, BLEND_ALPHA);
    EXPECT_EQ(compositor_math::to_blend_factors(LayerBlendMode::Additive).raylib_mode, BLEND_ADDITIVE);
    EXPECT_EQ(compositor_math::to_blend_factors(LayerBlendMode::Multiply).raylib_mode,
              BLEND_MULTIPLIED);
}

TEST(ToBlendFactorsTest, ScreenModeIsCustomWithScreenFormulaFactors) {
    const auto factors = compositor_math::to_blend_factors(LayerBlendMode::Screen);
    EXPECT_EQ(factors.raylib_mode, BLEND_CUSTOM);
    // result = src*(1-dst) + dst*1 = src + dst - src*dst
    EXPECT_EQ(factors.src_factor, RL_ONE_MINUS_DST_COLOR);
    EXPECT_EQ(factors.dst_factor, RL_ONE);
    EXPECT_EQ(factors.equation, RL_FUNC_ADD);
}

TEST(MoveElementTest, NoOpOnEmptyVector) {
    std::vector<int> v;
    compositor_math::move_element(v, 0, 0);
    EXPECT_TRUE(v.empty());
}

TEST(MoveElementTest, NoOpWhenFromEqualsTo) {
    std::vector<int> v{1, 2, 3};
    compositor_math::move_element(v, 1, 1);
    EXPECT_EQ(v, (std::vector<int>{1, 2, 3}));
}

TEST(MoveElementTest, MovesForward) {
    std::vector<int> v{1, 2, 3, 4};
    compositor_math::move_element(v, 0, 2);
    EXPECT_EQ(v, (std::vector<int>{2, 3, 1, 4}));
}

TEST(MoveElementTest, MovesBackward) {
    std::vector<int> v{1, 2, 3, 4};
    compositor_math::move_element(v, 3, 1);
    EXPECT_EQ(v, (std::vector<int>{1, 4, 2, 3}));
}

TEST(MoveElementTest, ClampsOutOfRangeIndicesToLastElement) {
    std::vector<int> v{1, 2, 3};
    compositor_math::move_element(v, 99, 0);
    EXPECT_EQ(v, (std::vector<int>{3, 1, 2}));
}

TEST(NextFreqBandTest, RingOrderIsGlobalThenZeroThroughLastBand) {
    constexpr std::size_t kNumBands = 16;
    EXPECT_EQ(compositor_math::next_freq_band(std::nullopt, kNumBands), std::optional<std::size_t>(0));
    EXPECT_EQ(compositor_math::next_freq_band(0, kNumBands), std::optional<std::size_t>(1));
    EXPECT_EQ(compositor_math::next_freq_band(14, kNumBands), std::optional<std::size_t>(15));
    EXPECT_EQ(compositor_math::next_freq_band(15, kNumBands), std::nullopt);
}

TEST(PrevFreqBandTest, RingOrderReversesNextFreqBand) {
    constexpr std::size_t kNumBands = 16;
    EXPECT_EQ(compositor_math::prev_freq_band(std::nullopt, kNumBands),
              std::optional<std::size_t>(15));
    EXPECT_EQ(compositor_math::prev_freq_band(15, kNumBands), std::optional<std::size_t>(14));
    EXPECT_EQ(compositor_math::prev_freq_band(1, kNumBands), std::optional<std::size_t>(0));
    EXPECT_EQ(compositor_math::prev_freq_band(0, kNumBands), std::nullopt);
}

TEST(DefaultReactivitySourceTest, GlobalBeatForGlobalBandOnsetForBand) {
    EXPECT_EQ(compositor_math::default_reactivity_source(std::nullopt), ReactivitySource::GlobalBeat);
    EXPECT_EQ(compositor_math::default_reactivity_source(3), ReactivitySource::BandOnset);
}

TEST(NextReactivitySourceTest, TogglesWithinValidPairForBandMode) {
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::BandOnset, 3),
              ReactivitySource::BandAmplitude);
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::BandAmplitude, 3),
              ReactivitySource::BandOnset);
}

TEST(NextReactivitySourceTest, TogglesWithinValidPairForGlobalMode) {
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::GlobalBeat, std::nullopt),
              ReactivitySource::PeakFreq);
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::PeakFreq, std::nullopt),
              ReactivitySource::GlobalBeat);
}

TEST(NextReactivitySourceTest, StaleValueFromOtherModeFallsBackToDefault) {
    // GlobalBeat is not a valid option while a specific band is selected;
    // this happens transiently right after freq_band changes mode, before
    // the caller applies default_reactivity_source.
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::GlobalBeat, 3),
              ReactivitySource::BandOnset);
}

TEST(PrevReactivitySourceTest, TogglesWithinValidPairForBandMode) {
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::BandOnset, 3),
              ReactivitySource::BandAmplitude);
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::BandAmplitude, 3),
              ReactivitySource::BandOnset);
}

TEST(PrevReactivitySourceTest, TogglesWithinValidPairForGlobalMode) {
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::GlobalBeat, std::nullopt),
              ReactivitySource::PeakFreq);
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::PeakFreq, std::nullopt),
              ReactivitySource::GlobalBeat);
}

TEST(PrevReactivitySourceTest, StaleValueFromOtherModeFallsBackToDefault) {
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::GlobalBeat, 3),
              ReactivitySource::BandOnset);
}

TEST(PrevReactivitySourceTest, IsInverseOfNextReactivitySourceWithinAValidPair) {
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::BandOnset, 3),
              compositor_math::next_reactivity_source(ReactivitySource::BandOnset, 3));
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::GlobalBeat, std::nullopt),
              compositor_math::next_reactivity_source(ReactivitySource::GlobalBeat, std::nullopt));
}
