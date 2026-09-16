#include "compositor/compositor.h"

#include <gtest/gtest.h>

#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

TEST(CompositorAddLayerTest, ReturnsSequentialIndicesAndGrowsStack) {
    Compositor compositor;
    EXPECT_EQ(compositor.add_layer(VisualType::Waveform), std::optional<std::size_t>(0));
    EXPECT_EQ(compositor.add_layer(VisualType::Spectrum), std::optional<std::size_t>(1));
    EXPECT_EQ(compositor.add_layer(VisualType::Waveform), std::optional<std::size_t>(2));
    ASSERT_EQ(compositor.layers().size(), 3u);
}

TEST(CompositorRemoveLayerTest, RemovesElementAtValidIndex) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);
    compositor.add_layer(VisualType::Waveform);

    compositor.remove_layer(1);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Waveform);
}

TEST(CompositorRemoveLayerTest, OutOfRangeIndexIsNoOp) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);

    compositor.remove_layer(2);
    EXPECT_EQ(compositor.layers().size(), 2u);

    compositor.remove_layer(99);
    EXPECT_EQ(compositor.layers().size(), 2u);
}

TEST(CompositorRemoveLayerTest, OutOfRangeIndexOnEmptyStackIsNoOp) {
    Compositor compositor;
    compositor.remove_layer(0);
    EXPECT_TRUE(compositor.layers().empty());
}

TEST(CompositorMoveLayerTest, MovesTopToBottom) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);

    compositor.move_layer(1, 0);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Waveform);
}

TEST(CompositorMoveLayerTest, NoOpWhenFromEqualsTo) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);

    compositor.move_layer(0, 0);

    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Spectrum);
}

TEST(CompositorAddLayerTest, BlockedAtMaxLayers) {
    Compositor compositor;
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        ASSERT_TRUE(compositor.add_layer(VisualType::Waveform).has_value());
    }
    ASSERT_EQ(compositor.layers().size(), Compositor::kMaxLayers);

    EXPECT_EQ(compositor.add_layer(VisualType::Waveform), std::nullopt);
    EXPECT_EQ(compositor.layers().size(), Compositor::kMaxLayers);
}

TEST(CompositorDuplicateLayerTest, CopiesFieldsAndAppendsAtEnd) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    std::size_t source_index = compositor.add_layer(VisualType::Spectrum).value();
    Layer& source = compositor.layers()[source_index];
    source.opacity = 0.4f;
    source.blend_mode = LayerBlendMode::Additive;
    source.tint_color = Color{10, 20, 30, 255};
    source.palette_override = Palette{{{0.0f, Color{1, 2, 3, 255}}, {1.0f, Color{4, 5, 6, 255}}}};
    source.freq_band = 3;
    source.reactivity_source = ReactivitySource::BandAmplitude;
    source.sensitivity = 2.5f;
    source.enabled = false;

    std::optional<std::size_t> new_index = compositor.duplicate_layer(source_index);

    ASSERT_TRUE(new_index.has_value());
    EXPECT_EQ(*new_index, compositor.layers().size() - 1);
    const Layer& copy = compositor.layers()[*new_index];
    EXPECT_EQ(copy.visual_type, VisualType::Spectrum);
    EXPECT_NE(copy.visual.get(), source.visual.get());
    EXPECT_EQ(copy.opacity, 0.4f);
    EXPECT_EQ(copy.blend_mode, LayerBlendMode::Additive);
    EXPECT_EQ(copy.tint_color.r, 10);
    ASSERT_TRUE(copy.palette_override.has_value());
    EXPECT_EQ(copy.palette_override->stops.size(), 2u);
    EXPECT_EQ(copy.palette_override->stops[1].color.r, 4);
    EXPECT_EQ(copy.freq_band, std::optional<std::size_t>(3));
    EXPECT_EQ(copy.reactivity_source, ReactivitySource::BandAmplitude);
    EXPECT_FLOAT_EQ(copy.sensitivity, 2.5f);
    EXPECT_FALSE(copy.enabled);
}

TEST(CompositorDuplicateLayerTest, WaveformParamsSurviveDuplication) {
    Compositor compositor;
    std::size_t source_index = compositor.add_layer(VisualType::Waveform).value();
    compositor.layers()[source_index].visual =
        std::make_unique<WaveformVisual>(WaveformParams{BLUE, 5.0f});

    std::optional<std::size_t> new_index = compositor.duplicate_layer(source_index);

    ASSERT_TRUE(new_index.has_value());
    auto* copy = dynamic_cast<WaveformVisual*>(compositor.layers()[*new_index].visual.get());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->params().line_color.r, BLUE.r);
    EXPECT_FLOAT_EQ(copy->params().line_thickness, 5.0f);
}

TEST(CompositorDuplicateLayerTest, OutOfRangeIndexIsNoOp) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);

    EXPECT_EQ(compositor.duplicate_layer(5), std::nullopt);
    EXPECT_EQ(compositor.layers().size(), 1u);
}

TEST(CompositorDuplicateLayerTest, BlockedAtMaxLayers) {
    Compositor compositor;
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        compositor.add_layer(VisualType::Waveform);
    }

    EXPECT_EQ(compositor.duplicate_layer(0), std::nullopt);
    EXPECT_EQ(compositor.layers().size(), Compositor::kMaxLayers);
}

TEST(CompositorSetLayerVisualTypeTest, SwapsVisualAndPreservesOtherFields) {
    Compositor compositor;
    std::size_t index = compositor.add_layer(VisualType::Waveform).value();
    Layer& layer = compositor.layers()[index];
    layer.opacity = 0.6f;
    layer.blend_mode = LayerBlendMode::Multiply;
    layer.freq_band = 7;

    compositor.set_layer_visual_type(index, VisualType::Spectrum);

    const Layer& updated = compositor.layers()[index];
    EXPECT_EQ(updated.visual_type, VisualType::Spectrum);
    EXPECT_NE(dynamic_cast<SpectrumVisual*>(updated.visual.get()), nullptr);
    EXPECT_EQ(updated.opacity, 0.6f);
    EXPECT_EQ(updated.blend_mode, LayerBlendMode::Multiply);
    EXPECT_EQ(updated.freq_band, std::optional<std::size_t>(7));
}

TEST(CompositorSetLayerVisualTypeTest, OutOfRangeIndexIsNoOp) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);

    compositor.set_layer_visual_type(5, VisualType::Spectrum);

    EXPECT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
}
