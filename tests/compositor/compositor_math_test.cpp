#include "compositor/compositor_math.h"

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "compositor/compositor.h"
#include "raylib.h"
#include "rlgl.h"

namespace {
Layer make_test_layer(std::uint64_t id) {
    Layer layer{VisualType::Waveform, make_visual(VisualType::Waveform)};
    layer.layer_id = id;
    return layer;
}
}

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

TEST(NextReactivitySourceTest, GlobalModeIsANoOpSinceGlobalBeatIsTheOnlyValidSource) {
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::GlobalBeat, std::nullopt),
              ReactivitySource::GlobalBeat);
}

TEST(NextReactivitySourceTest, StaleValueFromOtherModeFallsBackToDefault) {
    EXPECT_EQ(compositor_math::next_reactivity_source(ReactivitySource::GlobalBeat, 3),
              ReactivitySource::BandOnset);
}

TEST(PrevReactivitySourceTest, TogglesWithinValidPairForBandMode) {
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::BandOnset, 3),
              ReactivitySource::BandAmplitude);
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::BandAmplitude, 3),
              ReactivitySource::BandOnset);
}

TEST(PrevReactivitySourceTest, GlobalModeIsANoOpSinceGlobalBeatIsTheOnlyValidSource) {
    EXPECT_EQ(compositor_math::prev_reactivity_source(ReactivitySource::GlobalBeat, std::nullopt),
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

TEST(HasDuplicateLayerIdsTest, EmptyIsFalse) {
    std::vector<Layer> layers;
    EXPECT_FALSE(compositor_math::has_duplicate_layer_ids(layers));
}

TEST(HasDuplicateLayerIdsTest, AllUniqueIsFalse) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(1));
    layers.push_back(make_test_layer(2));
    layers.push_back(make_test_layer(3));
    EXPECT_FALSE(compositor_math::has_duplicate_layer_ids(layers));
}

TEST(HasDuplicateLayerIdsTest, RepeatedIdFromAHandEditedPresetIsDetected) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(5));
    layers.push_back(make_test_layer(2));
    layers.push_back(make_test_layer(5));
    EXPECT_TRUE(compositor_math::has_duplicate_layer_ids(layers));
}

TEST(HasInvalidLayerIdTest, ZeroIdIsInvalid) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(kInvalidLayerId));
    EXPECT_TRUE(compositor_math::has_invalid_layer_id(layers));
}

TEST(HasInvalidLayerIdTest, NonZeroIdsAreValid) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(1));
    layers.push_back(make_test_layer(42));
    EXPECT_FALSE(compositor_math::has_invalid_layer_id(layers));
}

TEST(NextLayerIdAfterLoadTest, EmptyStartsAtOne) {
    std::vector<Layer> layers;
    EXPECT_EQ(compositor_math::next_layer_id_after_load(layers), 1u);
}

TEST(NextLayerIdAfterLoadTest, ResumesFromOneAboveTheHighestLoadedId) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(3));
    layers.push_back(make_test_layer(9));
    layers.push_back(make_test_layer(5));
    EXPECT_EQ(compositor_math::next_layer_id_after_load(layers), 10u);
}

TEST(NextLayerIdAfterLoadTest, SaturatesInsteadOfWrappingPastUint64MaxOnAHandEditedHugeId) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(std::numeric_limits<std::uint64_t>::max()));
    EXPECT_EQ(compositor_math::next_layer_id_after_load(layers),
              std::numeric_limits<std::uint64_t>::max());
}

TEST(LayerPaletteColorTest, IsDeterministicForTheSameIndex) {
    Color a = compositor_math::layer_palette_color(3);
    Color b = compositor_math::layer_palette_color(3);
    EXPECT_EQ(a.r, b.r);
    EXPECT_EQ(a.g, b.g);
    EXPECT_EQ(a.b, b.b);
}

TEST(LayerPaletteColorTest, DiffersAcrossDistinctIndices) {
    Color a = compositor_math::layer_palette_color(0);
    Color b = compositor_math::layer_palette_color(Compositor::kMaxLayers / 2);
    EXPECT_FALSE(a.r == b.r && a.g == b.g && a.b == b.b);
}

TEST(LayerPaletteColorTest, IsNeverWhite) {
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        Color c = compositor_math::layer_palette_color(i);
        EXPECT_FALSE(c.r == 255 && c.g == 255 && c.b == 255) << "index " << i << " produced white";
    }
}

TEST(ReassignLayerTintsTest, AssignsADistinctNonWhiteTintPerLayer) {
    std::vector<Layer> layers;
    layers.push_back(make_test_layer(1));
    layers.push_back(make_test_layer(2));
    layers.push_back(make_test_layer(3));
    ASSERT_EQ(layers[0].tint_color.r, WHITE.r);

    compositor_math::reassign_layer_tints(layers);

    for (std::size_t i = 0; i < layers.size(); ++i) {
        Color expected = compositor_math::layer_palette_color(i);
        EXPECT_EQ(layers[i].tint_color.r, expected.r);
        EXPECT_EQ(layers[i].tint_color.g, expected.g);
        EXPECT_EQ(layers[i].tint_color.b, expected.b);
    }
    EXPECT_NE(layers[0].tint_color.g, layers[1].tint_color.g);
}

TEST(ReassignLayerTintsTest, HandlesEmptyVector) {
    std::vector<Layer> layers;
    compositor_math::reassign_layer_tints(layers);
    EXPECT_TRUE(layers.empty());
}
