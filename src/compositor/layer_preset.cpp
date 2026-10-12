#include "compositor/layer_preset.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "audio/audio_config.h"
#include "compositor/compositor.h"
#include "compositor/compositor_math.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/number_clamp.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"
#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

namespace {

constexpr int kSchemaVersion = 1;

template <typename Enum>
using EnumEntry = std::pair<Enum, const char*>;

constexpr std::array<EnumEntry<VisualType>, 7> kVisualTypeTable{{
    {VisualType::Waveform, "Waveform"},
    {VisualType::Spectrum, "Spectrum"},
    {VisualType::Radial, "Radial"},
    {VisualType::Particles, "Particles"},
    {VisualType::Lissajous, "Lissajous"},
    {VisualType::FlowField, "FlowField"},
    {VisualType::Fractal, "Fractal"},
}};

constexpr std::array<EnumEntry<LayerBlendMode>, 4> kBlendModeTable{{
    {LayerBlendMode::Normal, "Normal"},
    {LayerBlendMode::Additive, "Additive"},
    {LayerBlendMode::Multiply, "Multiply"},
    {LayerBlendMode::Screen, "Screen"},
}};

constexpr std::array<EnumEntry<ReactivitySource>, 3> kReactivitySourceTable{{
    {ReactivitySource::GlobalBeat, "GlobalBeat"},
    {ReactivitySource::BandOnset, "BandOnset"},
    {ReactivitySource::BandAmplitude, "BandAmplitude"},
}};

template <typename Enum, std::size_t N>
const char* enum_to_string(Enum value, const std::array<EnumEntry<Enum>, N>& table) {
    for (const auto& entry : table) {
        if (entry.first == value) {
            return entry.second;
        }
    }
    throw std::invalid_argument("unmapped enum value");
}

template <typename Enum, std::size_t N>
Enum string_to_enum(const std::string& text, const std::array<EnumEntry<Enum>, N>& table) {
    for (const auto& entry : table) {
        if (text == entry.second) {
            return entry.first;
        }
    }
    throw std::invalid_argument("unknown enum string \"" + text + "\"");
}

double require_number_field(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_number()) {
        throw std::invalid_argument(std::string(key) + " must be a number");
    }
    return field.get<double>();
}

float require_float_field(const nlohmann::json& j, const char* key) {
    return number_math::clamp_number_to_representable<float>(require_number_field(j, key));
}

int require_byte_field(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_number_integer() && !field.is_number_unsigned()) {
        throw std::invalid_argument(std::string(key) + " must be an integer in [0, 255]");
    }
    const double value = field.get<double>();
    if (value < 0.0 || value > 255.0) {
        throw std::invalid_argument(std::string(key) + " must be an integer in [0, 255]");
    }
    return static_cast<int>(value);
}

std::uint64_t require_unsigned_field(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_number_unsigned()) {
        throw std::invalid_argument(std::string(key) + " must be a non-negative integer");
    }
    return field.get<std::uint64_t>();
}

Color color_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("color must be an object with r/g/b/a fields");
    }
    return Color{static_cast<unsigned char>(require_byte_field(j, "r")),
                 static_cast<unsigned char>(require_byte_field(j, "g")),
                 static_cast<unsigned char>(require_byte_field(j, "b")),
                 static_cast<unsigned char>(require_byte_field(j, "a"))};
}

nlohmann::json color_to_json(const Color& c) {
    return nlohmann::json{{"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a}};
}

Palette palette_from_json(const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("stops") || !j.at("stops").is_array()) {
        throw std::invalid_argument("palette_override must be an object with a stops array");
    }
    Palette palette;
    for (const nlohmann::json& stop_json : j.at("stops")) {
        if (!stop_json.is_object()) {
            throw std::invalid_argument("palette stop must be an object");
        }
        PaletteStop stop;
        stop.position = require_float_field(stop_json, "position");
        stop.color = color_from_json(stop_json.at("color"));
        palette.stops.push_back(stop);
    }
    return palette;
}

nlohmann::json palette_to_json(const Palette& palette) {
    nlohmann::json stops = nlohmann::json::array();
    for (const PaletteStop& stop : palette.stops) {
        stops.push_back(nlohmann::json{{"position", stop.position}, {"color", color_to_json(stop.color)}});
    }
    return nlohmann::json{{"stops", stops}};
}

nlohmann::json params_to_json(const WaveformParams& p) {
    return nlohmann::json{{"line_color", color_to_json(p.line_color)}, {"line_thickness", p.line_thickness}};
}

WaveformParams waveform_params_from_json(const nlohmann::json& j) {
    WaveformParams p;
    p.line_color = color_from_json(j.at("line_color"));
    p.line_thickness = require_float_field(j, "line_thickness");
    return p;
}

nlohmann::json params_to_json(const SpectrumParams& p) {
    return nlohmann::json{{"bar_color", color_to_json(p.bar_color)},
                          {"gap_ratio", p.gap_ratio},
                          {"max_height_fraction", p.max_height_fraction}};
}

SpectrumParams spectrum_params_from_json(const nlohmann::json& j) {
    SpectrumParams p;
    p.bar_color = color_from_json(j.at("bar_color"));
    p.gap_ratio = require_float_field(j, "gap_ratio");
    p.max_height_fraction = require_float_field(j, "max_height_fraction");
    return p;
}

nlohmann::json params_to_json(const RadialParams& p) {
    return nlohmann::json{{"bar_color", color_to_json(p.bar_color)},
                          {"thickness", p.thickness},
                          {"inner_radius_fraction", p.inner_radius_fraction},
                          {"max_radius_fraction", p.max_radius_fraction}};
}

RadialParams radial_params_from_json(const nlohmann::json& j) {
    RadialParams p;
    p.bar_color = color_from_json(j.at("bar_color"));
    p.thickness = require_float_field(j, "thickness");
    p.max_radius_fraction =
        radial_math::clamp_max_radius_fraction(require_number_field(j, "max_radius_fraction"), 0.0f);
    p.inner_radius_fraction = radial_math::clamp_inner_radius_fraction(
        require_number_field(j, "inner_radius_fraction"), p.max_radius_fraction);
    return p;
}

nlohmann::json params_to_json(const ParticlesParams& p) {
    return nlohmann::json{{"particle_color", color_to_json(p.particle_color)},
                          {"radius", p.radius},
                          {"lifetime_frames", p.lifetime_frames},
                          {"burst_count", p.burst_count},
                          {"spawn_rate_per_frame", p.spawn_rate_per_frame},
                          {"inner_radius_fraction", p.inner_radius_fraction}};
}

ParticlesParams particles_params_from_json(const nlohmann::json& j) {
    ParticlesParams p;
    p.particle_color = color_from_json(j.at("particle_color"));
    p.radius = require_float_field(j, "radius");
    p.lifetime_frames =
        particles_math::clamp_lifetime_frames_from_number(require_number_field(j, "lifetime_frames"));
    p.burst_count = particles_math::clamp_burst_count_from_number(require_number_field(j, "burst_count"));
    p.spawn_rate_per_frame = require_float_field(j, "spawn_rate_per_frame");
    p.inner_radius_fraction =
        particles_math::clamp_inner_radius_fraction(require_number_field(j, "inner_radius_fraction"));
    return p;
}

nlohmann::json params_to_json(const LissajousParams& p) {
    return nlohmann::json{{"line_color", color_to_json(p.line_color)},
                          {"line_thickness", p.line_thickness},
                          {"compression_exponent", p.compression_exponent}};
}

LissajousParams lissajous_params_from_json(const nlohmann::json& j) {
    LissajousParams p;
    p.line_color = color_from_json(j.at("line_color"));
    p.line_thickness = require_float_field(j, "line_thickness");
    p.compression_exponent =
        lissajous_math::clamp_compression_exponent(require_number_field(j, "compression_exponent"));
    return p;
}

nlohmann::json params_to_json(const FlowFieldParams& p) {
    return nlohmann::json{{"particle_color", color_to_json(p.particle_color)},
                          {"radius", p.radius},
                          {"lifetime_frames", p.lifetime_frames},
                          {"burst_count", p.burst_count},
                          {"spawn_rate_per_frame", p.spawn_rate_per_frame},
                          {"ripple_on_onset_enabled", p.ripple_on_onset_enabled},
                          {"burst_on_onset_enabled", p.burst_on_onset_enabled},
                          {"turbulence_frequency", p.turbulence_frequency},
                          {"evolution_per_frame", p.evolution_per_frame},
                          {"grid_cols", p.grid_cols}};
}

FlowFieldParams flow_field_params_from_json(const nlohmann::json& j) {
    FlowFieldParams p;
    p.particle_color = color_from_json(j.at("particle_color"));
    p.radius = require_float_field(j, "radius");
    p.lifetime_frames =
        particles_math::clamp_lifetime_frames_from_number(require_number_field(j, "lifetime_frames"));
    p.burst_count = particles_math::clamp_burst_count_from_number(require_number_field(j, "burst_count"));
    p.spawn_rate_per_frame = require_float_field(j, "spawn_rate_per_frame");
    p.ripple_on_onset_enabled = j.at("ripple_on_onset_enabled").get<bool>();
    p.burst_on_onset_enabled = j.at("burst_on_onset_enabled").get<bool>();
    p.turbulence_frequency =
        flow_field_math::clamp_turbulence_frequency(require_float_field(j, "turbulence_frequency"));
    p.evolution_per_frame = flow_field_math::clamp_evolution_per_frame(require_float_field(j, "evolution_per_frame"));
    p.grid_cols = flow_field_math::clamp_grid_cols_from_number(require_number_field(j, "grid_cols"));
    return p;
}

nlohmann::json params_to_json(const FractalParams& p) {
    return nlohmann::json{{"base_zoom", p.base_zoom},
                          {"zoom_amplitude_scale", p.zoom_amplitude_scale},
                          {"julia_base_radius", p.julia_base_radius},
                          {"julia_radius_amplitude_scale", p.julia_radius_amplitude_scale},
                          {"julia_angular_speed_radians_per_sec", p.julia_angular_speed_radians_per_sec},
                          {"julia_orbit_center_x", p.julia_orbit_center_x},
                          {"julia_orbit_center_y", p.julia_orbit_center_y},
                          {"julia_orbit_clockwise", p.julia_orbit_clockwise},
                          {"color_cycle_speed_hz", p.color_cycle_speed_hz},
                          {"amplitude_smoothing_tau_ms", p.amplitude_smoothing_tau_ms},
                          {"boundary_margin_px", p.boundary_margin_px}};
}

FractalParams fractal_params_from_json(const nlohmann::json& j) {
    FractalParams p;
    p.base_zoom = fractal_math::clamp_base_zoom(require_number_field(j, "base_zoom"));
    p.zoom_amplitude_scale =
        fractal_math::clamp_zoom_amplitude_scale(require_number_field(j, "zoom_amplitude_scale"));
    p.julia_base_radius = fractal_math::clamp_julia_base_radius(require_number_field(j, "julia_base_radius"));
    p.julia_radius_amplitude_scale = fractal_math::clamp_julia_radius_amplitude_scale(
        require_number_field(j, "julia_radius_amplitude_scale"));
    p.julia_angular_speed_radians_per_sec =
        fractal_math::clamp_julia_angular_speed(require_number_field(j, "julia_angular_speed_radians_per_sec"));
    p.julia_orbit_center_x =
        fractal_math::clamp_julia_orbit_center(require_number_field(j, "julia_orbit_center_x"));
    p.julia_orbit_center_y =
        fractal_math::clamp_julia_orbit_center(require_number_field(j, "julia_orbit_center_y"));
    p.julia_orbit_clockwise = j.at("julia_orbit_clockwise").get<bool>();
    p.color_cycle_speed_hz =
        fractal_math::clamp_color_cycle_speed_hz(require_number_field(j, "color_cycle_speed_hz"));
    p.amplitude_smoothing_tau_ms = require_float_field(j, "amplitude_smoothing_tau_ms");
    p.boundary_margin_px = require_float_field(j, "boundary_margin_px");
    return p;
}

std::unique_ptr<VisualModule> make_visual_from_params_json(VisualType type,
                                                             const nlohmann::json& params_json) {
    if (!params_json.is_object()) {
        throw std::invalid_argument("params must be an object");
    }
    switch (type) {
        case VisualType::Waveform:
            return std::make_unique<WaveformVisual>(waveform_params_from_json(params_json));
        case VisualType::Spectrum:
            return std::make_unique<SpectrumVisual>(spectrum_params_from_json(params_json));
        case VisualType::Radial:
            return std::make_unique<RadialVisual>(radial_params_from_json(params_json));
        case VisualType::Particles:
            return std::make_unique<ParticlesVisual>(particles_params_from_json(params_json));
        case VisualType::Lissajous:
            return std::make_unique<LissajousVisual>(lissajous_params_from_json(params_json));
        case VisualType::FlowField:
            return std::make_unique<FlowFieldVisual>(flow_field_params_from_json(params_json));
        case VisualType::Fractal:
            return std::make_unique<FractalVisual>(fractal_params_from_json(params_json));
    }
    throw std::invalid_argument("unknown VisualType");
}

nlohmann::json params_json_for_layer(const Layer& layer) {
    switch (layer.visual_type) {
        case VisualType::Waveform:
            return params_to_json(dynamic_cast<WaveformVisual&>(*layer.visual).params());
        case VisualType::Spectrum:
            return params_to_json(dynamic_cast<SpectrumVisual&>(*layer.visual).params());
        case VisualType::Radial:
            return params_to_json(dynamic_cast<RadialVisual&>(*layer.visual).params());
        case VisualType::Particles:
            return params_to_json(dynamic_cast<ParticlesVisual&>(*layer.visual).params());
        case VisualType::Lissajous:
            return params_to_json(dynamic_cast<LissajousVisual&>(*layer.visual).params());
        case VisualType::FlowField:
            return params_to_json(dynamic_cast<FlowFieldVisual&>(*layer.visual).params());
        case VisualType::Fractal:
            return params_to_json(dynamic_cast<FractalVisual&>(*layer.visual).params());
    }
    throw std::invalid_argument("unknown VisualType");
}

nlohmann::json layer_to_json(const Layer& layer) {
    nlohmann::json j;
    j["layer_id"] = layer.layer_id;
    j["visual_type"] = enum_to_string(layer.visual_type, kVisualTypeTable);
    j["enabled"] = layer.enabled;
    j["opacity"] = layer.opacity;
    j["blend_mode"] = enum_to_string(layer.blend_mode, kBlendModeTable);
    j["tint_color"] = color_to_json(layer.tint_color);
    j["palette_override"] =
        layer.palette_override.has_value() ? palette_to_json(*layer.palette_override) : nlohmann::json(nullptr);
    j["freq_band"] = layer.freq_band.has_value() ? nlohmann::json(*layer.freq_band) : nlohmann::json(nullptr);
    j["reactivity_source"] = enum_to_string(layer.reactivity_source, kReactivitySourceTable);
    j["sensitivity"] = layer.sensitivity;
    j["params"] = params_json_for_layer(layer);
    return j;
}

Layer layer_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("layer entry must be an object");
    }

    const std::uint64_t layer_id = require_unsigned_field(j, "layer_id");
    if (layer_id == kInvalidLayerId) {
        throw std::invalid_argument("layer_id must not be zero");
    }

    const VisualType visual_type =
        string_to_enum<VisualType>(j.at("visual_type").get<std::string>(), kVisualTypeTable);

    Layer layer{visual_type, make_visual_from_params_json(visual_type, j.at("params"))};
    layer.layer_id = layer_id;
    layer.enabled = j.at("enabled").get<bool>();
    layer.opacity = require_float_field(j, "opacity");
    layer.blend_mode = string_to_enum<LayerBlendMode>(j.at("blend_mode").get<std::string>(), kBlendModeTable);
    layer.tint_color = color_from_json(j.at("tint_color"));

    const nlohmann::json& palette_json = j.at("palette_override");
    layer.palette_override = palette_json.is_null() ? std::nullopt
                                                      : std::optional<Palette>(palette_from_json(palette_json));

    const nlohmann::json& freq_band_json = j.at("freq_band");
    if (freq_band_json.is_null()) {
        layer.freq_band = std::nullopt;
    } else {
        if (!freq_band_json.is_number_unsigned()) {
            throw std::invalid_argument("freq_band must be null or a non-negative integer");
        }
        const std::uint64_t band = freq_band_json.get<std::uint64_t>();
        if (band >= audio_config::kNumBands) {
            throw std::invalid_argument("freq_band out of range");
        }
        layer.freq_band = static_cast<std::size_t>(band);
    }

    layer.reactivity_source =
        string_to_enum<ReactivitySource>(j.at("reactivity_source").get<std::string>(), kReactivitySourceTable);
    layer.sensitivity = require_float_field(j, "sensitivity");

    return layer;
}

}

namespace layer_preset {

std::string serialize(const std::vector<Layer>& layers) {
    nlohmann::json doc;
    doc["schema_version"] = kSchemaVersion;
    nlohmann::json layers_json = nlohmann::json::array();
    for (const Layer& layer : layers) {
        layers_json.push_back(layer_to_json(layer));
    }
    doc["layers"] = layers_json;
    return doc.dump(2);
}

std::optional<std::vector<Layer>> deserialize(const std::string& json_text) {
    try {
        const nlohmann::json doc = nlohmann::json::parse(json_text);
        if (!doc.is_object()) {
            return std::nullopt;
        }
        if (require_number_field(doc, "schema_version") != static_cast<double>(kSchemaVersion)) {
            return std::nullopt;
        }
        const nlohmann::json& layers_json = doc.at("layers");
        if (!layers_json.is_array()) {
            return std::nullopt;
        }

        std::vector<Layer> layers;
        layers.reserve(layers_json.size());
        for (const nlohmann::json& layer_json : layers_json) {
            layers.push_back(layer_from_json(layer_json));
        }

        if (layers.size() > Compositor::kMaxLayers) {
            return std::nullopt;
        }
        if (compositor_math::has_duplicate_layer_ids(layers)) {
            return std::nullopt;
        }

        return layers;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

}
