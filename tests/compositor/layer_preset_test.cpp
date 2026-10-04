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

TEST(LayerPresetAdversarialTest, FloatFieldsBeyondFloatRangeLoadAsFiniteValues) {
    nlohmann::json doc = serialized_single_particles_doc();
    doc["layers"][0]["opacity"] = 1e300;
    doc["layers"][0]["sensitivity"] = -1e300;
    doc["layers"][0]["params"]["radius"] = 1e300;
    doc["layers"][0]["params"]["spawn_rate_per_frame"] = -1e300;

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(doc.dump());

    ASSERT_TRUE(loaded.has_value());
    const Layer& layer = (*loaded)[0];
    const ParticlesParams& params = dynamic_cast<ParticlesVisual&>(*layer.visual).params();
    EXPECT_EQ(layer.opacity, std::numeric_limits<float>::max());
    EXPECT_EQ(layer.sensitivity, std::numeric_limits<float>::lowest());
    EXPECT_EQ(params.radius, std::numeric_limits<float>::max());
    EXPECT_EQ(params.spawn_rate_per_frame, std::numeric_limits<float>::lowest());
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
