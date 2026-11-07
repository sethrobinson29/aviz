#include "compositor/compositor.h"

#include <cstdint>
#include <unordered_set>

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
    source.name = "Kick pulse";

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
    EXPECT_EQ(copy.name, "Kick pulse");
}

TEST(CompositorDuplicateLayerTest, UnnamedLayerDuplicatesAsUnnamed) {
    Compositor compositor;
    std::size_t source_index = compositor.add_layer(VisualType::Waveform).value();

    std::size_t copy_index = compositor.duplicate_layer(source_index).value();

    EXPECT_TRUE(compositor.layers()[copy_index].name.empty());
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

TEST(CompositorLayerIdTest, AddLayerAssignsANonZeroId) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);

    EXPECT_NE(compositor.layers()[0].layer_id, kInvalidLayerId);
}

TEST(CompositorLayerIdTest, EachAddedLayerGetsADistinctId) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);
    compositor.add_layer(VisualType::Waveform);

    EXPECT_NE(compositor.layers()[0].layer_id, compositor.layers()[1].layer_id);
    EXPECT_NE(compositor.layers()[1].layer_id, compositor.layers()[2].layer_id);
    EXPECT_NE(compositor.layers()[0].layer_id, compositor.layers()[2].layer_id);
}

TEST(CompositorLayerIdTest, DuplicateGetsAFreshIdNotTheSourcesId) {
    Compositor compositor;
    std::size_t source_index = compositor.add_layer(VisualType::Waveform).value();
    std::uint64_t source_id = compositor.layers()[source_index].layer_id;

    std::size_t copy_index = compositor.duplicate_layer(source_index).value();

    EXPECT_NE(compositor.layers()[copy_index].layer_id, source_id);
    EXPECT_NE(compositor.layers()[copy_index].layer_id, kInvalidLayerId);
}

TEST(CompositorLayerIdTest, RemovingThenAddingNeverReusesARetiredId) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t retired_id = compositor.layers()[0].layer_id;
    compositor.remove_layer(0);

    compositor.add_layer(VisualType::Waveform);

    EXPECT_NE(compositor.layers()[0].layer_id, retired_id);
}

TEST(CompositorLayerIdTest, ManyAddDuplicateAndRemoveCyclesNeverProduceADuplicateId) {
    Compositor compositor;
    std::unordered_set<std::uint64_t> ever_assigned;

    for (int cycle = 0; cycle < 500; ++cycle) {
        std::optional<std::size_t> added = compositor.add_layer(VisualType::Waveform);
        if (added.has_value()) {
            std::uint64_t id = compositor.layers()[*added].layer_id;
            ASSERT_TRUE(ever_assigned.insert(id).second) << "duplicate layer_id: " << id;
        }

        if (!compositor.layers().empty()) {
            std::optional<std::size_t> duplicated = compositor.duplicate_layer(0);
            if (duplicated.has_value()) {
                std::uint64_t id = compositor.layers()[*duplicated].layer_id;
                ASSERT_TRUE(ever_assigned.insert(id).second) << "duplicate layer_id: " << id;
            }
        }

        if (cycle % 3 == 0 && !compositor.layers().empty()) {
            compositor.remove_layer(compositor.layers().size() - 1);
        }
    }
}

TEST(CompositorIndexOfLayerTest, FindsExistingLayerById) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    std::size_t second = compositor.add_layer(VisualType::Spectrum).value();
    std::uint64_t second_id = compositor.layers()[second].layer_id;

    EXPECT_EQ(compositor.index_of_layer(second_id), std::optional<std::size_t>(second));
}

TEST(CompositorIndexOfLayerTest, ReturnsNulloptForUnknownId) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);

    EXPECT_EQ(compositor.index_of_layer(99999), std::nullopt);
}

TEST(CompositorIndexOfLayerTest, ReturnsNulloptForInvalidLayerId) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);

    EXPECT_EQ(compositor.index_of_layer(kInvalidLayerId), std::nullopt);
}

TEST(CompositorIndexOfLayerTest, TracksCurrentIndexAfterReorder) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    std::size_t second = compositor.add_layer(VisualType::Spectrum).value();
    std::uint64_t second_id = compositor.layers()[second].layer_id;

    compositor.move_layer(second, 0);

    EXPECT_EQ(compositor.index_of_layer(second_id), std::optional<std::size_t>(0));
}

TEST(CompositorReplaceLayersTest, SucceedsAndReplacesStackContents) {
    Compositor compositor;
    compositor.add_layer(VisualType::Spectrum);

    std::vector<Layer> loaded;
    loaded.push_back(Layer{VisualType::Waveform, make_visual(VisualType::Waveform)});
    loaded[0].layer_id = 7;

    EXPECT_TRUE(compositor.replace_layers_if_ids_valid(std::move(loaded)));
    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor.layers()[0].layer_id, 7u);
}

TEST(CompositorReplaceLayersTest, ResumesIdCounterFromOneAboveTheHighestLoadedId) {
    Compositor compositor;

    std::vector<Layer> loaded;
    loaded.push_back(Layer{VisualType::Waveform, make_visual(VisualType::Waveform)});
    loaded[0].layer_id = 41;
    loaded.push_back(Layer{VisualType::Spectrum, make_visual(VisualType::Spectrum)});
    loaded[1].layer_id = 12;

    ASSERT_TRUE(compositor.replace_layers_if_ids_valid(std::move(loaded)));
    EXPECT_EQ(compositor.next_layer_id(), 42u);

    compositor.add_layer(VisualType::Waveform);
    EXPECT_EQ(compositor.layers().back().layer_id, 42u);
}

TEST(CompositorReplaceLayersTest, RejectsDuplicateIdsAndLeavesStackUnchanged) {
    Compositor compositor;
    compositor.add_layer(VisualType::Spectrum);

    std::vector<Layer> loaded;
    loaded.push_back(Layer{VisualType::Waveform, make_visual(VisualType::Waveform)});
    loaded[0].layer_id = 3;
    loaded.push_back(Layer{VisualType::Waveform, make_visual(VisualType::Waveform)});
    loaded[1].layer_id = 3;

    EXPECT_FALSE(compositor.replace_layers_if_ids_valid(std::move(loaded)));
    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
}

TEST(CompositorReplaceLayersTest, RejectsInvalidZeroIdAndLeavesStackUnchanged) {
    Compositor compositor;
    compositor.add_layer(VisualType::Spectrum);

    std::vector<Layer> loaded;
    loaded.push_back(Layer{VisualType::Waveform, make_visual(VisualType::Waveform)});
    loaded[0].layer_id = kInvalidLayerId;

    EXPECT_FALSE(compositor.replace_layers_if_ids_valid(std::move(loaded)));
    ASSERT_EQ(compositor.layers().size(), 1u);
}

TEST(CompositorReplaceLayersTest, RejectsMoreThanMaxLayersAndLeavesStackUnchanged) {
    Compositor compositor;
    compositor.add_layer(VisualType::Spectrum);

    std::vector<Layer> loaded;
    for (std::size_t i = 0; i < Compositor::kMaxLayers + 1; ++i) {
        loaded.push_back(Layer{VisualType::Waveform, make_visual(VisualType::Waveform)});
        loaded.back().layer_id = i + 1;
    }

    EXPECT_FALSE(compositor.replace_layers_if_ids_valid(std::move(loaded)));
    ASSERT_EQ(compositor.layers().size(), 1u);
}
