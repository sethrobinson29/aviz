#include "compositor/layer_preset.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "audio/audio_config.h"
#include "compositor/compositor.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"
#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/visual_config.h"
#include "visuals/waveform/waveform_visual.h"
#include "compositor/compositor_math.h"

namespace {

nlohmann::json minimal_waveform_layer_json(std::uint64_t id) {
    return nlohmann::json{
        {"layer_id", id},
        {"visual_type", "Waveform"},
        {"enabled", true},
        {"opacity", 1.0f},
        {"blend_mode", "Normal"},
        {"tint_color", nlohmann::json{{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
        {"palette_override", nullptr},
        {"freq_band", nullptr},
        {"reactivity_source", "GlobalBeat"},
        {"sensitivity", 1.0f},
        {"params", nlohmann::json{{"line_color", nlohmann::json{{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
                                   {"line_thickness", 3.0f}}},
    };
}

std::string wrap_doc(nlohmann::json layers_array) {
    nlohmann::json doc{{"schema_version", 1}, {"layers", std::move(layers_array)}};
    return doc.dump();
}

}

TEST(LayerPresetRoundTripTest, SingleLayerPreservesSharedFields) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Spectrum, std::make_unique<SpectrumVisual>(SpectrumParams{RED, 0.3f, 0.7f})};
    layer.layer_id = 7;
    layer.opacity = 0.6f;
    layer.blend_mode = LayerBlendMode::Multiply;
    layer.tint_color = Color{11, 22, 33, 44};
    layer.freq_band = 5;
    layer.reactivity_source = ReactivitySource::BandOnset;
    layer.sensitivity = 2.5f;
    layer.enabled = false;
    layers.push_back(std::move(layer));

    std::string json_text = layer_preset::serialize(layers);
    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(json_text);

    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->size(), 1u);
    const Layer& result = (*loaded)[0];
    EXPECT_EQ(result.layer_id, 7u);
    EXPECT_EQ(result.visual_type, VisualType::Spectrum);
    EXPECT_FLOAT_EQ(result.opacity, 0.6f);
    EXPECT_EQ(result.blend_mode, LayerBlendMode::Multiply);
    EXPECT_EQ(result.tint_color.r, 11);
    EXPECT_EQ(result.tint_color.g, 22);
    EXPECT_EQ(result.tint_color.b, 33);
    EXPECT_EQ(result.tint_color.a, 44);
    EXPECT_EQ(result.freq_band, std::optional<std::size_t>(5));
    EXPECT_EQ(result.reactivity_source, ReactivitySource::BandOnset);
    EXPECT_FLOAT_EQ(result.sensitivity, 2.5f);
    EXPECT_FALSE(result.enabled);
}

TEST(LayerPresetRoundTripTest, NullFreqBandRoundTripsAsGlobal) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Waveform, std::make_unique<WaveformVisual>()};
    layer.layer_id = 1;
    layer.freq_band = std::nullopt;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ((*loaded)[0].freq_band, std::nullopt);
}

TEST(LayerPresetRoundTripTest, PaletteOverrideRoundTrips) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Waveform, std::make_unique<WaveformVisual>()};
    layer.layer_id = 1;
    layer.palette_override =
        Palette{{{0.0f, Color{1, 2, 3, 255}}, {1.0f, Color{4, 5, 6, 255}}}};
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE((*loaded)[0].palette_override.has_value());
    EXPECT_EQ((*loaded)[0].palette_override->stops.size(), 2u);
    EXPECT_EQ((*loaded)[0].palette_override->stops[1].color.r, 4);
}

TEST(LayerPresetRoundTripTest, NullPaletteOverrideRoundTrips) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Waveform, std::make_unique<WaveformVisual>()};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE((*loaded)[0].palette_override.has_value());
}

TEST(LayerPresetRoundTripTest, MultipleLayersPreserveOrder) {
    std::vector<Layer> layers;
    Layer a{VisualType::Waveform, std::make_unique<WaveformVisual>()};
    a.layer_id = 1;
    Layer b{VisualType::Spectrum, std::make_unique<SpectrumVisual>()};
    b.layer_id = 2;
    Layer c{VisualType::Radial, std::make_unique<RadialVisual>()};
    c.layer_id = 3;
    layers.push_back(std::move(a));
    layers.push_back(std::move(b));
    layers.push_back(std::move(c));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->size(), 3u);
    EXPECT_EQ((*loaded)[0].visual_type, VisualType::Waveform);
    EXPECT_EQ((*loaded)[1].visual_type, VisualType::Spectrum);
    EXPECT_EQ((*loaded)[2].visual_type, VisualType::Radial);
    EXPECT_EQ((*loaded)[0].layer_id, 1u);
    EXPECT_EQ((*loaded)[1].layer_id, 2u);
    EXPECT_EQ((*loaded)[2].layer_id, 3u);
}

TEST(LayerPresetRoundTripTest, WaveformParamsRoundTrip) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Waveform, std::make_unique<WaveformVisual>(WaveformParams{BLUE, 5.0f})};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    auto* visual = dynamic_cast<WaveformVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(visual, nullptr);
    EXPECT_EQ(visual->params().line_color.r, BLUE.r);
    EXPECT_FLOAT_EQ(visual->params().line_thickness, 5.0f);
}

TEST(LayerPresetRoundTripTest, ParticlesParamsRoundTrip) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Particles,
                std::make_unique<ParticlesVisual>(ParticlesParams{ORANGE, 5.0f, 90, 30, 3.0f, 0.2f})};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    auto* visual = dynamic_cast<ParticlesVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(visual, nullptr);
    EXPECT_EQ(visual->params().particle_color.r, ORANGE.r);
    EXPECT_FLOAT_EQ(visual->params().radius, 5.0f);
    EXPECT_EQ(visual->params().lifetime_frames, 90);
    EXPECT_EQ(visual->params().burst_count, 30);
    EXPECT_FLOAT_EQ(visual->params().spawn_rate_per_frame, 3.0f);
    EXPECT_FLOAT_EQ(visual->params().inner_radius_fraction, 0.2f);
}

TEST(LayerPresetRoundTripTest, FlowFieldParamsRoundTrip) {
    FlowFieldParams params;
    params.particle_color = SKYBLUE;
    params.radius = 1.5f;
    params.lifetime_frames = 500;
    params.burst_count = 10;
    params.spawn_rate_per_frame = 0.4f;
    params.ripple_on_onset_enabled = false;
    params.burst_on_onset_enabled = false;
    params.turbulence_frequency = 0.03f;
    params.evolution_per_frame = 0.02f;
    params.grid_cols = 16;
    std::vector<Layer> layers;
    Layer layer{VisualType::FlowField, std::make_unique<FlowFieldVisual>(params)};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    auto* visual = dynamic_cast<FlowFieldVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(visual, nullptr);
    EXPECT_FALSE(visual->params().ripple_on_onset_enabled);
    EXPECT_FALSE(visual->params().burst_on_onset_enabled);
    EXPECT_EQ(visual->params().lifetime_frames, 500);
    EXPECT_FLOAT_EQ(visual->params().turbulence_frequency, 0.03f);
    EXPECT_FLOAT_EQ(visual->params().evolution_per_frame, 0.02f);
    EXPECT_EQ(visual->params().grid_cols, 16);
}

namespace {

nlohmann::json serialized_single_flow_field_doc() {
    std::vector<Layer> layers;
    Layer layer{VisualType::FlowField, std::make_unique<FlowFieldVisual>()};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));
    return nlohmann::json::parse(layer_preset::serialize(layers));
}

}

TEST(LayerPresetAdversarialTest, RejectsFlowFieldParamsMissingAnyFieldShapeField) {
    for (const char* key : {"turbulence_frequency", "evolution_per_frame", "grid_cols"}) {
        nlohmann::json doc = serialized_single_flow_field_doc();
        doc["layers"][0]["params"].erase(key);
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << key;
    }
}

TEST(LayerPresetAdversarialTest, RejectsFlowFieldGridColsOfWrongType) {
    nlohmann::json doc = serialized_single_flow_field_doc();
    doc["layers"][0]["params"]["grid_cols"] = "32";
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, ClampsOutOfRangeFlowFieldFieldShapeValuesOnLoad) {
    nlohmann::json doc = serialized_single_flow_field_doc();
    doc["layers"][0]["params"]["grid_cols"] = 100000;
    doc["layers"][0]["params"]["turbulence_frequency"] = 50.0;
    doc["layers"][0]["params"]["evolution_per_frame"] = -4.0;

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

    ASSERT_TRUE(loaded.has_value());
    auto* visual = dynamic_cast<FlowFieldVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(visual, nullptr);
    EXPECT_EQ(visual->params().grid_cols, visual_config::kFlowFieldGridColsMax);
    EXPECT_FLOAT_EQ(visual->params().turbulence_frequency, visual_config::kFlowFieldTurbulenceFrequencyMax);
    EXPECT_FLOAT_EQ(visual->params().evolution_per_frame, visual_config::kFlowFieldEvolutionPerFrameMin);
}

TEST(LayerPresetRoundTripTest, FractalParamsRoundTrip) {
    FractalParams params;
    params.base_zoom = 1.5f;
    params.julia_orbit_clockwise = false;
    params.boundary_margin_px = 25.0f;
    std::vector<Layer> layers;
    Layer layer{VisualType::Fractal, std::make_unique<FractalVisual>(params)};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    auto* visual = dynamic_cast<FractalVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(visual, nullptr);
    EXPECT_FLOAT_EQ(visual->params().base_zoom, 1.5f);
    EXPECT_FALSE(visual->params().julia_orbit_clockwise);
    EXPECT_FLOAT_EQ(visual->params().boundary_margin_px, 25.0f);
}

TEST(LayerPresetRoundTripTest, SavingNeverProducesDuplicateIdsFromAValidInMemoryStack) {
    Compositor compositor;
    for (int i = 0; i < 10; ++i) {
        compositor.add_layer(VisualType::Waveform);
    }

    std::string json_text = layer_preset::serialize(compositor.layers());
    nlohmann::json doc = nlohmann::json::parse(json_text);

    std::vector<std::uint64_t> ids;
    for (const auto& layer_json : doc.at("layers")) {
        ids.push_back(layer_json.at("layer_id").get<std::uint64_t>());
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(std::adjacent_find(ids.begin(), ids.end()), ids.end());
}

TEST(LayerPresetAdversarialTest, RejectsInvalidJsonText) {
    EXPECT_EQ(layer_preset::deserialize("{ this is not json"), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsNonObjectTopLevel) {
    EXPECT_EQ(layer_preset::deserialize("[1, 2, 3]"), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsWrongSchemaVersion) {
    nlohmann::json doc{{"schema_version", 999}, {"layers", nlohmann::json::array({minimal_waveform_layer_json(1)})}};
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsMissingLayersField) {
    nlohmann::json doc{{"schema_version", 1}};
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsLayersFieldThatIsNotAnArray) {
    nlohmann::json doc{{"schema_version", 1}, {"layers", "not-an-array"}};
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsDuplicateLayerIdsAcrossLayers) {
    nlohmann::json layers = nlohmann::json::array({minimal_waveform_layer_json(5), minimal_waveform_layer_json(5)});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsZeroLayerId) {
    nlohmann::json layers = nlohmann::json::array({minimal_waveform_layer_json(0)});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsNegativeLayerId) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["layer_id"] = -5;
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsMissingLayerIdField) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer.erase("layer_id");
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsUnknownVisualTypeString) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["visual_type"] = "TotallyNotAVisual";
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsUnknownBlendModeString) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["blend_mode"] = "Xor";
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsUnknownReactivitySourceString) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["reactivity_source"] = "Vibes";
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsFreqBandAtOrAboveNumBands) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["freq_band"] = audio_config::kNumBands;
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsWildlyOutOfRangeFreqBandFromAHandEditedFile) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["freq_band"] = 999999999;
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsNegativeFreqBand) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["freq_band"] = -1;
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsColorByteAboveTwoFiveFive) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["tint_color"]["r"] = 300;
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsNegativeColorByte) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["tint_color"]["r"] = -1;
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsMissingParamsField) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer.erase("params");
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsParamsMismatchedToVisualType) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["visual_type"] = "Fractal";
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsMoreLayersThanMaxLayers) {
    nlohmann::json layers = nlohmann::json::array();
    for (std::uint64_t i = 1; i <= Compositor::kMaxLayers + 1; ++i) {
        layers.push_back(minimal_waveform_layer_json(i));
    }
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, OneMalformedLayerAmongManyValidOnesRejectsTheWholeFile) {
    nlohmann::json bad_layer = minimal_waveform_layer_json(2);
    bad_layer["blend_mode"] = "NotReal";
    nlohmann::json layers =
        nlohmann::json::array({minimal_waveform_layer_json(1), bad_layer, minimal_waveform_layer_json(3)});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsPaletteOverrideThatIsNotObjectOrNull) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["palette_override"] = "not-a-palette";
    nlohmann::json layers = nlohmann::json::array({layer});
    EXPECT_EQ(layer_preset::deserialize(wrap_doc(layers)), std::nullopt);
}

namespace {

nlohmann::json serialized_single_particles_doc() {
    std::vector<Layer> layers;
    Layer layer{VisualType::Particles, std::make_unique<ParticlesVisual>()};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));
    return nlohmann::json::parse(layer_preset::serialize(layers));
}

}

TEST(LayerPresetAdversarialTest, ExtremeFloatLifetimeFramesLoadsClampedForParticles) {
    nlohmann::json doc = serialized_single_particles_doc();

    doc["layers"][0]["params"]["lifetime_frames"] = 1e30;
    std::optional<std::vector<Layer>> high = layer_preset::deserialize(doc.dump());
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(dynamic_cast<ParticlesVisual&>(*(*high)[0].visual).params().lifetime_frames,
              visual_config::kParticlesLifetimeFramesMax);

    doc["layers"][0]["params"]["lifetime_frames"] = -1e30;
    std::optional<std::vector<Layer>> low = layer_preset::deserialize(doc.dump());
    ASSERT_TRUE(low.has_value());
    EXPECT_EQ(dynamic_cast<ParticlesVisual&>(*(*low)[0].visual).params().lifetime_frames,
              visual_config::kParticlesLifetimeFramesMin);
}

TEST(LayerPresetAdversarialTest, ExtremeFloatLifetimeFramesAndGridColsLoadClampedForFlowField) {
    nlohmann::json doc = serialized_single_flow_field_doc();

    doc["layers"][0]["params"]["lifetime_frames"] = 1e30;
    doc["layers"][0]["params"]["grid_cols"] = 1e30;
    std::optional<std::vector<Layer>> high = layer_preset::deserialize(doc.dump());
    ASSERT_TRUE(high.has_value());
    const FlowFieldParams& high_params = dynamic_cast<FlowFieldVisual&>(*(*high)[0].visual).params();
    EXPECT_EQ(high_params.lifetime_frames, visual_config::kParticlesLifetimeFramesMax);
    EXPECT_EQ(high_params.grid_cols, visual_config::kFlowFieldGridColsMax);

    doc["layers"][0]["params"]["lifetime_frames"] = -1e30;
    doc["layers"][0]["params"]["grid_cols"] = -1e30;
    std::optional<std::vector<Layer>> low = layer_preset::deserialize(doc.dump());
    ASSERT_TRUE(low.has_value());
    const FlowFieldParams& low_params = dynamic_cast<FlowFieldVisual&>(*(*low)[0].visual).params();
    EXPECT_EQ(low_params.lifetime_frames, visual_config::kParticlesLifetimeFramesMin);
    EXPECT_EQ(low_params.grid_cols, visual_config::kFlowFieldGridColsMin);
}

TEST(LayerPresetAdversarialTest, IntegerBeyondIntRangeLoadsClampedForLifetimeFramesAndGridCols) {
    nlohmann::json doc = serialized_single_flow_field_doc();
    doc["layers"][0]["params"]["lifetime_frames"] = 99999999999999LL;
    doc["layers"][0]["params"]["grid_cols"] = 99999999999999LL;

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

    ASSERT_TRUE(loaded.has_value());
    const FlowFieldParams& params = dynamic_cast<FlowFieldVisual&>(*(*loaded)[0].visual).params();
    EXPECT_EQ(params.lifetime_frames, visual_config::kParticlesLifetimeFramesMax);
    EXPECT_EQ(params.grid_cols, visual_config::kFlowFieldGridColsMax);
}

TEST(LayerPresetAdversarialTest, RejectsNonNumericLifetimeFrames) {
    nlohmann::json doc = serialized_single_particles_doc();
    doc["layers"][0]["params"]["lifetime_frames"] = "150";
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, ExtremeBurstCountLoadsClampedForParticlesAndFlowField) {
    nlohmann::json particles_doc = serialized_single_particles_doc();
    particles_doc["layers"][0]["params"]["burst_count"] = 1e30;
    std::optional<std::vector<Layer>> particles_high = layer_preset::deserialize(particles_doc.dump());
    ASSERT_TRUE(particles_high.has_value());
    EXPECT_EQ(dynamic_cast<ParticlesVisual&>(*(*particles_high)[0].visual).params().burst_count,
              visual_config::kMaxParticles);

    particles_doc["layers"][0]["params"]["burst_count"] = -1e30;
    std::optional<std::vector<Layer>> particles_low = layer_preset::deserialize(particles_doc.dump());
    ASSERT_TRUE(particles_low.has_value());
    EXPECT_EQ(dynamic_cast<ParticlesVisual&>(*(*particles_low)[0].visual).params().burst_count, 0);

    nlohmann::json flow_doc = serialized_single_flow_field_doc();
    flow_doc["layers"][0]["params"]["burst_count"] = 99999999999999LL;
    std::optional<std::vector<Layer>> flow_high = layer_preset::deserialize(flow_doc.dump());
    ASSERT_TRUE(flow_high.has_value());
    EXPECT_EQ(dynamic_cast<FlowFieldVisual&>(*(*flow_high)[0].visual).params().burst_count,
              visual_config::kMaxParticles);
}

TEST(LayerPresetAdversarialTest, ExtremeFloatFieldsLoadClampedToEachFieldsOwnRange) {
    nlohmann::json doc = serialized_single_particles_doc();
    doc["layers"][0]["opacity"] = 1e300;
    doc["layers"][0]["sensitivity"] = -1e300;
    doc["layers"][0]["params"]["radius"] = 1e300;
    doc["layers"][0]["params"]["spawn_rate_per_frame"] = -1e300;

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

    ASSERT_TRUE(loaded.has_value());
    const Layer& layer = (*loaded)[0];
    const ParticlesParams& params = dynamic_cast<ParticlesVisual&>(*layer.visual).params();
    EXPECT_FLOAT_EQ(layer.opacity, 1.0f);
    EXPECT_FLOAT_EQ(layer.sensitivity, compositor_math::kSensitivityMin);
    EXPECT_FLOAT_EQ(params.radius, visual_config::kParticleRadiusPxMax);
    EXPECT_FLOAT_EQ(params.spawn_rate_per_frame, visual_config::kParticlesSpawnRatePerFrameMin);
}

TEST(LayerPresetAdversarialTest, SpawnRatePerFrameLoadsClampedToThePanelRangeForParticlesAndFlowField) {
    nlohmann::json particles_doc = serialized_single_particles_doc();
    particles_doc["layers"][0]["params"]["spawn_rate_per_frame"] = 1e300;
    std::optional<std::vector<Layer>> particles = layer_preset::deserialize(particles_doc.dump());
    ASSERT_TRUE(particles.has_value());
    EXPECT_FLOAT_EQ(dynamic_cast<ParticlesVisual&>(*(*particles)[0].visual).params().spawn_rate_per_frame,
                    visual_config::kParticlesSpawnRatePerFrameMax);

    nlohmann::json flow_doc = serialized_single_flow_field_doc();
    flow_doc["layers"][0]["params"]["spawn_rate_per_frame"] = 1e300;
    std::optional<std::vector<Layer>> flow = layer_preset::deserialize(flow_doc.dump());
    ASSERT_TRUE(flow.has_value());
    EXPECT_FLOAT_EQ(dynamic_cast<FlowFieldVisual&>(*(*flow)[0].visual).params().spawn_rate_per_frame,
                    visual_config::kFlowFieldSpawnRatePerFrameMax);

    flow_doc["layers"][0]["params"]["spawn_rate_per_frame"] = -3.0;
    flow = layer_preset::deserialize(flow_doc.dump());
    ASSERT_TRUE(flow.has_value());
    EXPECT_FLOAT_EQ(dynamic_cast<FlowFieldVisual&>(*(*flow)[0].visual).params().spawn_rate_per_frame,
                    visual_config::kFlowFieldSpawnRatePerFrameMin);
}

TEST(LayerPresetAdversarialTest, RejectsBooleanWhereANumberIsRequired) {
    nlohmann::json doc = serialized_single_particles_doc();
    doc["layers"][0]["opacity"] = true;
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);

    doc = serialized_single_particles_doc();
    doc["layers"][0]["params"]["burst_count"] = true;
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsFractionalOrExtremeSchemaVersion) {
    nlohmann::json doc = serialized_single_particles_doc();
    doc["schema_version"] = 1.5;
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);

    doc["schema_version"] = 1e30;
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsColorByteThatWouldWrapIntoRangeWhenNarrowed) {
    nlohmann::json doc = serialized_single_particles_doc();
    doc["layers"][0]["tint_color"]["r"] = 4294967301LL;
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

namespace {

nlohmann::json serialized_single_lissajous_doc() {
    std::vector<Layer> layers;
    Layer layer{VisualType::Lissajous, std::make_unique<LissajousVisual>()};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));
    return nlohmann::json::parse(layer_preset::serialize(layers));
}

}

TEST(LayerPresetRoundTripTest, LissajousParamsRoundTrip) {
    std::vector<Layer> layers;
    Layer layer{VisualType::Lissajous, std::make_unique<LissajousVisual>(LissajousParams{GREEN, 4.0f, 0.8f})};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    auto* visual = dynamic_cast<LissajousVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(visual, nullptr);
    EXPECT_EQ(visual->params().line_color.g, GREEN.g);
    EXPECT_FLOAT_EQ(visual->params().line_thickness, 4.0f);
    EXPECT_FLOAT_EQ(visual->params().compression_exponent, 0.8f);
}

TEST(LayerPresetAdversarialTest, RejectsLissajousParamsMissingCompressionExponent) {
    nlohmann::json doc = serialized_single_lissajous_doc();
    doc["layers"][0]["params"].erase("compression_exponent");
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsLissajousCompressionExponentOfWrongType) {
    for (const nlohmann::json& bad : {nlohmann::json("0.5"), nlohmann::json(true), nlohmann::json(nullptr)}) {
        nlohmann::json doc = serialized_single_lissajous_doc();
        doc["layers"][0]["params"]["compression_exponent"] = bad;
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << bad.dump();
    }
}

TEST(LayerPresetAdversarialTest, OutOfRangeAndExtremeLissajousCompressionExponentLoadsClamped) {
    const std::pair<double, float> cases[] = {
        {0.0, visual_config::kLissajousCompressionExponentMin},
        {-2.0, visual_config::kLissajousCompressionExponentMin},
        {-1e300, visual_config::kLissajousCompressionExponentMin},
        {5.0, visual_config::kLissajousCompressionExponentMax},
        {1e300, visual_config::kLissajousCompressionExponentMax},
    };
    for (const auto& [input, expected] : cases) {
        nlohmann::json doc = serialized_single_lissajous_doc();
        doc["layers"][0]["params"]["compression_exponent"] = input;

        std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

        ASSERT_TRUE(loaded.has_value()) << input;
        EXPECT_FLOAT_EQ(dynamic_cast<LissajousVisual&>(*(*loaded)[0].visual).params().compression_exponent, expected)
            << input;
    }
}

namespace {

nlohmann::json serialized_single_radial_doc() {
    std::vector<Layer> layers;
    Layer layer{VisualType::Radial, std::make_unique<RadialVisual>()};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));
    return nlohmann::json::parse(layer_preset::serialize(layers));
}

const RadialParams& load_radial_params(const nlohmann::json& doc, std::optional<std::vector<Layer>>& loaded) {
    loaded = layer_preset::deserialize(doc.dump());
    return dynamic_cast<RadialVisual&>(*loaded->at(0).visual).params();
}

}

TEST(LayerPresetAdversarialTest, ValidRadialRadiusPairLoadsUnchanged) {
    nlohmann::json doc = serialized_single_radial_doc();
    doc["layers"][0]["params"]["inner_radius_fraction"] = 0.3;
    doc["layers"][0]["params"]["max_radius_fraction"] = 0.6;

    std::optional<std::vector<Layer>> loaded;
    const RadialParams& params = load_radial_params(doc, loaded);

    EXPECT_FLOAT_EQ(params.inner_radius_fraction, 0.3f);
    EXPECT_FLOAT_EQ(params.max_radius_fraction, 0.6f);
}

TEST(LayerPresetAdversarialTest, InvertedRadialRadiusPairLoadsWithInnerPulledBelowOuter) {
    nlohmann::json doc = serialized_single_radial_doc();
    doc["layers"][0]["params"]["inner_radius_fraction"] = 0.8;
    doc["layers"][0]["params"]["max_radius_fraction"] = 0.4;

    std::optional<std::vector<Layer>> loaded;
    const RadialParams& params = load_radial_params(doc, loaded);

    EXPECT_FLOAT_EQ(params.max_radius_fraction, 0.4f);
    EXPECT_NEAR(params.inner_radius_fraction, 0.4f - radial_math::kMinRadiusGapFraction, 1e-6f);
}

TEST(LayerPresetAdversarialTest, OutOfRangeAndExtremeRadialRadiusFractionsLoadClampedWithGapKept) {
    nlohmann::json doc = serialized_single_radial_doc();
    std::optional<std::vector<Layer>> loaded;

    doc["layers"][0]["params"]["inner_radius_fraction"] = 1e300;
    doc["layers"][0]["params"]["max_radius_fraction"] = 1e300;
    const RadialParams& high = load_radial_params(doc, loaded);
    EXPECT_FLOAT_EQ(high.max_radius_fraction, 1.0f);
    EXPECT_FLOAT_EQ(high.inner_radius_fraction, 1.0f - radial_math::kMinRadiusGapFraction);

    doc["layers"][0]["params"]["inner_radius_fraction"] = -1e300;
    doc["layers"][0]["params"]["max_radius_fraction"] = -1e300;
    const RadialParams& low = load_radial_params(doc, loaded);
    EXPECT_FLOAT_EQ(low.max_radius_fraction, radial_math::kMinRadiusGapFraction);
    EXPECT_FLOAT_EQ(low.inner_radius_fraction, 0.0f);

    doc["layers"][0]["params"]["inner_radius_fraction"] = 5.0;
    doc["layers"][0]["params"]["max_radius_fraction"] = -1.0;
    const RadialParams& crossed = load_radial_params(doc, loaded);
    EXPECT_FLOAT_EQ(crossed.max_radius_fraction, radial_math::kMinRadiusGapFraction);
    EXPECT_FLOAT_EQ(crossed.inner_radius_fraction, 0.0f);
}

TEST(LayerPresetAdversarialTest, RejectsRadialRadiusFractionsOfWrongTypeOrMissing) {
    for (const char* key : {"inner_radius_fraction", "max_radius_fraction"}) {
        for (const nlohmann::json& bad : {nlohmann::json("0.5"), nlohmann::json(true), nlohmann::json(nullptr)}) {
            nlohmann::json doc = serialized_single_radial_doc();
            doc["layers"][0]["params"][key] = bad;
            EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << key << " " << bad.dump();
        }
        nlohmann::json doc = serialized_single_radial_doc();
        doc["layers"][0]["params"].erase(key);
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << key;
    }
}

TEST(LayerPresetAdversarialTest, OutOfRangeAndExtremeParticlesInnerRadiusFractionLoadsClamped) {
    const std::pair<double, float> cases[] = {
        {0.35, 0.35f}, {-0.5, 0.0f}, {-1e300, 0.0f}, {1.5, 1.0f}, {1e300, 1.0f},
    };
    for (const auto& [input, expected] : cases) {
        nlohmann::json doc = serialized_single_particles_doc();
        doc["layers"][0]["params"]["inner_radius_fraction"] = input;

        std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

        ASSERT_TRUE(loaded.has_value()) << input;
        EXPECT_FLOAT_EQ(dynamic_cast<ParticlesVisual&>(*(*loaded)[0].visual).params().inner_radius_fraction, expected)
            << input;
    }
}

TEST(LayerPresetAdversarialTest, RejectsParticlesInnerRadiusFractionOfWrongType) {
    for (const nlohmann::json& bad : {nlohmann::json("0.5"), nlohmann::json(true), nlohmann::json(nullptr)}) {
        nlohmann::json doc = serialized_single_particles_doc();
        doc["layers"][0]["params"]["inner_radius_fraction"] = bad;
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << bad.dump();
    }
}

namespace {

struct FractalPresetFieldCase {
    const char* key;
    float FractalParams::* field;
    float in_range_value;
    float min;
    float max;
};

const FractalPresetFieldCase kFractalPresetFieldCases[] = {
    {"base_zoom", &FractalParams::base_zoom, 2.5f, visual_config::kFractalBaseZoomMin,
     visual_config::kFractalBaseZoomMax},
    {"zoom_amplitude_scale", &FractalParams::zoom_amplitude_scale, 1.25f,
     visual_config::kFractalZoomAmplitudeScaleMin, visual_config::kFractalZoomAmplitudeScaleMax},
    {"julia_base_radius", &FractalParams::julia_base_radius, 0.75f, visual_config::kFractalJuliaBaseRadiusMin,
     visual_config::kFractalJuliaBaseRadiusMax},
    {"julia_radius_amplitude_scale", &FractalParams::julia_radius_amplitude_scale, 0.12f,
     visual_config::kFractalJuliaRadiusAmplitudeScaleMin, visual_config::kFractalJuliaRadiusAmplitudeScaleMax},
    {"julia_angular_speed_radians_per_sec", &FractalParams::julia_angular_speed_radians_per_sec, 0.6f,
     visual_config::kFractalJuliaAngularSpeedRadiansPerSecMin,
     visual_config::kFractalJuliaAngularSpeedRadiansPerSecMax},
    {"julia_orbit_center_x", &FractalParams::julia_orbit_center_x, 0.2f,
     visual_config::kFractalJuliaOrbitCenterMin, visual_config::kFractalJuliaOrbitCenterMax},
    {"julia_orbit_center_y", &FractalParams::julia_orbit_center_y, -0.3f,
     visual_config::kFractalJuliaOrbitCenterMin, visual_config::kFractalJuliaOrbitCenterMax},
    {"color_cycle_speed_hz", &FractalParams::color_cycle_speed_hz, 0.3f,
     visual_config::kFractalColorCycleSpeedHzMin, visual_config::kFractalColorCycleSpeedHzMax},
};

nlohmann::json serialized_single_fractal_doc() {
    std::vector<Layer> layers;
    Layer layer{VisualType::Fractal, std::make_unique<FractalVisual>()};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));
    return nlohmann::json::parse(layer_preset::serialize(layers));
}

}

TEST(LayerPresetRoundTripTest, EveryExposedFractalParamRoundTripsWhenInRange) {
    FractalParams params;
    for (const FractalPresetFieldCase& c : kFractalPresetFieldCases) {
        params.*(c.field) = c.in_range_value;
    }
    params.julia_orbit_clockwise = false;
    std::vector<Layer> layers;
    Layer layer{VisualType::Fractal, std::make_unique<FractalVisual>(params)};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(layer_preset::serialize(layers));

    ASSERT_TRUE(loaded.has_value());
    const FractalParams& loaded_params = dynamic_cast<FractalVisual&>(*(*loaded)[0].visual).params();
    for (const FractalPresetFieldCase& c : kFractalPresetFieldCases) {
        EXPECT_FLOAT_EQ(loaded_params.*(c.field), c.in_range_value) << c.key;
    }
    EXPECT_FALSE(loaded_params.julia_orbit_clockwise);
}

TEST(LayerPresetAdversarialTest, OutOfRangeAndExtremeFractalParamsLoadClamped) {
    for (const FractalPresetFieldCase& c : kFractalPresetFieldCases) {
        const std::pair<double, float> cases[] = {
            {static_cast<double>(c.min) - 0.25, c.min},
            {-1e300, c.min},
            {static_cast<double>(c.max) + 0.25, c.max},
            {1e300, c.max},
        };
        for (const auto& [input, expected] : cases) {
            nlohmann::json doc = serialized_single_fractal_doc();
            doc["layers"][0]["params"][c.key] = input;

            std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

            ASSERT_TRUE(loaded.has_value()) << c.key << " input " << input;
            EXPECT_FLOAT_EQ(dynamic_cast<FractalVisual&>(*(*loaded)[0].visual).params().*(c.field), expected)
                << c.key << " input " << input;
        }
    }
}

TEST(LayerPresetAdversarialTest, FractalBaseZoomBelowOneLoadsAsOne) {
    nlohmann::json doc = serialized_single_fractal_doc();
    doc["layers"][0]["params"]["base_zoom"] = 0.25;

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FLOAT_EQ(dynamic_cast<FractalVisual&>(*(*loaded)[0].visual).params().base_zoom, 1.0f);
}

TEST(LayerPresetAdversarialTest, RejectsFractalFloatParamsOfWrongTypeOrMissing) {
    for (const FractalPresetFieldCase& c : kFractalPresetFieldCases) {
        for (const nlohmann::json& bad : {nlohmann::json("0.5"), nlohmann::json(true), nlohmann::json(nullptr)}) {
            nlohmann::json doc = serialized_single_fractal_doc();
            doc["layers"][0]["params"][c.key] = bad;
            EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << c.key << " " << bad.dump();
        }
        nlohmann::json doc = serialized_single_fractal_doc();
        doc["layers"][0]["params"].erase(c.key);
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << c.key << " missing";
    }
}

TEST(LayerPresetAdversarialTest, RejectsFractalOrbitClockwiseOfWrongTypeOrMissing) {
    for (const nlohmann::json& bad : {nlohmann::json("true"), nlohmann::json(1), nlohmann::json(nullptr)}) {
        nlohmann::json doc = serialized_single_fractal_doc();
        doc["layers"][0]["params"]["julia_orbit_clockwise"] = bad;
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << bad.dump();
    }
    nlohmann::json doc = serialized_single_fractal_doc();
    doc["layers"][0]["params"].erase("julia_orbit_clockwise");
    EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt);
}

namespace {

struct PresetParamRangeCase {
    VisualType type;
    const char* key;
    float min;
    float max;
};

const PresetParamRangeCase kPresetParamRangeCases[] = {
    {VisualType::Waveform, "line_thickness", visual_config::kLineThicknessPxMin,
     visual_config::kLineThicknessPxMax},
    {VisualType::Lissajous, "line_thickness", visual_config::kLineThicknessPxMin,
     visual_config::kLineThicknessPxMax},
    {VisualType::Spectrum, "gap_ratio", visual_config::kSpectrumGapRatioMin, visual_config::kSpectrumGapRatioMax},
    {VisualType::Spectrum, "max_height_fraction", visual_config::kSpectrumMaxHeightFractionMin,
     visual_config::kSpectrumMaxHeightFractionMax},
    {VisualType::Radial, "thickness", visual_config::kRadialThicknessPxMin, visual_config::kRadialThicknessPxMax},
    {VisualType::Particles, "radius", visual_config::kParticleRadiusPxMin, visual_config::kParticleRadiusPxMax},
    {VisualType::Particles, "spawn_rate_per_frame", visual_config::kParticlesSpawnRatePerFrameMin,
     visual_config::kParticlesSpawnRatePerFrameMax},
    {VisualType::FlowField, "radius", visual_config::kParticleRadiusPxMin, visual_config::kParticleRadiusPxMax},
    {VisualType::FlowField, "spawn_rate_per_frame", visual_config::kFlowFieldSpawnRatePerFrameMin,
     visual_config::kFlowFieldSpawnRatePerFrameMax},
    {VisualType::Fractal, "amplitude_smoothing_tau_ms", visual_config::kFractalAmplitudeSmoothingTauMsMin,
     visual_config::kFractalAmplitudeSmoothingTauMsMax},
    {VisualType::Fractal, "boundary_margin_px", visual_config::kFractalBoundaryMarginPxMin,
     visual_config::kFractalBoundaryMarginPxMax},
};

nlohmann::json serialized_single_default_layer_doc(VisualType type) {
    std::vector<Layer> layers;
    Layer layer{type, make_visual(type)};
    layer.layer_id = 1;
    layers.push_back(std::move(layer));
    return nlohmann::json::parse(layer_preset::serialize(layers));
}

float reserialized_param(const std::vector<Layer>& layers, const char* key) {
    return nlohmann::json::parse(layer_preset::serialize(layers))["layers"][0]["params"][key].get<float>();
}

}

TEST(LayerPresetAdversarialTest, PreviouslyUnboundedParamsLoadClampedToTheirRangeForEveryVisualType) {
    for (const PresetParamRangeCase& c : kPresetParamRangeCases) {
        const std::pair<double, float> cases[] = {
            {static_cast<double>(c.min) - 1.0, c.min},
            {-1e300, c.min},
            {static_cast<double>(c.max) + 1.0, c.max},
            {1e300, c.max},
        };
        for (const auto& [input, expected] : cases) {
            nlohmann::json doc = serialized_single_default_layer_doc(c.type);
            ASSERT_TRUE(doc["layers"][0]["params"].contains(c.key)) << c.key;
            doc["layers"][0]["params"][c.key] = input;

            std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

            ASSERT_TRUE(loaded.has_value()) << c.key << " input " << input;
            EXPECT_FLOAT_EQ(reserialized_param(*loaded, c.key), expected) << c.key << " input " << input;
        }
    }
}

TEST(LayerPresetAdversarialTest, PreviouslyUnboundedParamsInsideTheirRangeLoadUnchanged) {
    for (const PresetParamRangeCase& c : kPresetParamRangeCases) {
        const float inside = c.min + (c.max - c.min) * 0.25f;
        nlohmann::json doc = serialized_single_default_layer_doc(c.type);
        doc["layers"][0]["params"][c.key] = inside;

        std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

        ASSERT_TRUE(loaded.has_value()) << c.key;
        EXPECT_FLOAT_EQ(reserialized_param(*loaded, c.key), inside) << c.key;
    }
}

TEST(LayerPresetAdversarialTest, PreviouslyUnboundedParamsOfWrongTypeOrMissingAreRejected) {
    for (const PresetParamRangeCase& c : kPresetParamRangeCases) {
        for (const nlohmann::json& bad : {nlohmann::json("1.0"), nlohmann::json(true), nlohmann::json(nullptr)}) {
            nlohmann::json doc = serialized_single_default_layer_doc(c.type);
            doc["layers"][0]["params"][c.key] = bad;
            EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << c.key << " " << bad.dump();
        }
        nlohmann::json doc = serialized_single_default_layer_doc(c.type);
        doc["layers"][0]["params"].erase(c.key);
        EXPECT_EQ(layer_preset::deserialize(doc.dump()), std::nullopt) << c.key << " missing";
    }
}

TEST(LayerPresetAdversarialTest, SpectrumGapRatioCannotLoadAsTheValueThatDividesBarWidthByZero) {
    nlohmann::json doc = serialized_single_default_layer_doc(VisualType::Spectrum);
    doc["layers"][0]["params"]["gap_ratio"] = -1.0;

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FLOAT_EQ(dynamic_cast<SpectrumVisual&>(*(*loaded)[0].visual).params().gap_ratio,
                    visual_config::kSpectrumGapRatioMin);
}

TEST(LayerPresetAdversarialTest, OpacityAndSensitivityLoadClampedToTheirPanelRanges) {
    const std::pair<double, float> opacity_cases[] = {{-0.5, 0.0f}, {-1e300, 0.0f}, {1.5, 1.0f}, {1e300, 1.0f}};
    for (const auto& [input, expected] : opacity_cases) {
        nlohmann::json layer = minimal_waveform_layer_json(1);
        layer["opacity"] = input;
        std::optional<std::vector<Layer>> loaded =
            layer_preset::deserialize(wrap_doc(nlohmann::json::array({layer})));
        ASSERT_TRUE(loaded.has_value()) << input;
        EXPECT_FLOAT_EQ((*loaded)[0].opacity, expected) << input;
    }

    const std::pair<double, float> sensitivity_cases[] = {{-1.0, compositor_math::kSensitivityMin},
                                                          {-1e300, compositor_math::kSensitivityMin},
                                                          {9.0, compositor_math::kSensitivityMax},
                                                          {1e300, compositor_math::kSensitivityMax}};
    for (const auto& [input, expected] : sensitivity_cases) {
        nlohmann::json layer = minimal_waveform_layer_json(1);
        layer["sensitivity"] = input;
        std::optional<std::vector<Layer>> loaded =
            layer_preset::deserialize(wrap_doc(nlohmann::json::array({layer})));
        ASSERT_TRUE(loaded.has_value()) << input;
        EXPECT_FLOAT_EQ((*loaded)[0].sensitivity, expected) << input;
    }
}

namespace {

nlohmann::json palette_stop_json(double position) {
    return nlohmann::json{{"position", position},
                          {"color", nlohmann::json{{"r", 10}, {"g", 20}, {"b", 30}, {"a", 255}}}};
}

std::optional<std::vector<Layer>> load_waveform_layer_with_palette_stops(nlohmann::json stops) {
    nlohmann::json layer = minimal_waveform_layer_json(1);
    layer["palette_override"] = nlohmann::json{{"stops", std::move(stops)}};
    return layer_preset::deserialize(wrap_doc(nlohmann::json::array({layer})));
}

}

TEST(LayerPresetAdversarialTest, RejectsPaletteOverrideWithNoStops) {
    EXPECT_EQ(load_waveform_layer_with_palette_stops(nlohmann::json::array()), std::nullopt);
}

TEST(LayerPresetAdversarialTest, RejectsPaletteOverrideWhoseStopsAreNotInAscendingOrder) {
    EXPECT_EQ(load_waveform_layer_with_palette_stops(
                  nlohmann::json::array({palette_stop_json(0.8), palette_stop_json(0.2)})),
              std::nullopt);
    EXPECT_EQ(load_waveform_layer_with_palette_stops(nlohmann::json::array(
                  {palette_stop_json(0.0), palette_stop_json(0.6), palette_stop_json(0.5)})),
              std::nullopt);
}

TEST(LayerPresetAdversarialTest, PaletteStopPositionsOutsideUnitRangeLoadClamped) {
    std::optional<std::vector<Layer>> loaded = load_waveform_layer_with_palette_stops(
        nlohmann::json::array({palette_stop_json(-5.0), palette_stop_json(0.5), palette_stop_json(1e300)}));

    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE((*loaded)[0].palette_override.has_value());
    const std::vector<PaletteStop>& stops = (*loaded)[0].palette_override->stops;
    ASSERT_EQ(stops.size(), 3u);
    EXPECT_FLOAT_EQ(stops[0].position, 0.0f);
    EXPECT_FLOAT_EQ(stops[1].position, 0.5f);
    EXPECT_FLOAT_EQ(stops[2].position, 1.0f);
}

TEST(LayerPresetAdversarialTest, AcceptsSingleStopAndEqualPositionPaletteOverrides) {
    EXPECT_TRUE(load_waveform_layer_with_palette_stops(nlohmann::json::array({palette_stop_json(0.3)})).has_value());
    EXPECT_TRUE(load_waveform_layer_with_palette_stops(
                    nlohmann::json::array({palette_stop_json(0.5), palette_stop_json(0.5)}))
                    .has_value());
}
