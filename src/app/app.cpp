#include "app/app.h"

#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/audio_source_controller.h"
#include "app/layer_generator.h"
#include "app/raylib_window_watcher.h"
#include "app/window_layout_config.h"
#include "app/window_layout_controller.h"
#include "audio/audio_config.h"
#include "audio/audio_features.h"
#include "audio/feature_worker.h"
#include "audio/file_producer.h"
#include "audio/producer_host.h"
#include "audio/ring_buffer.h"
#include "compositor/compositor.h"
#include "compositor/compositor_math.h"
#include "compositor/layer.h"
#include "compositor/layer_preset.h"
#include "compositor/preset_file_io.h"
#include "raylib.h"
#include "ui_host/panel_message_apply.h"
#include "ui_host/ui_host.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"

namespace {
constexpr int kTargetFps = 60;

const char* kDebugPresetFilename = "debug-preset.json";

std::string resolve_absolute_panel_content_dir() {
    const char* app_dir = GetApplicationDirectory();
    std::string candidate = std::string(app_dir) + "web/panel";
    std::string resolved = DirectoryExists(candidate.c_str()) ? candidate : std::string("web/panel");
    return std::filesystem::absolute(resolved).string();
}

const char* blend_mode_name(LayerBlendMode mode) {
    switch (mode) {
        case LayerBlendMode::Normal:
            return "Normal";
        case LayerBlendMode::Additive:
            return "Additive";
        case LayerBlendMode::Multiply:
            return "Multiply";
        case LayerBlendMode::Screen:
            return "Screen";
    }
    return "?";
}

const char* visual_type_name(VisualType type) {
    switch (type) {
        case VisualType::Waveform:
            return "Waveform";
        case VisualType::Spectrum:
            return "Spectrum";
        case VisualType::Radial:
            return "Radial";
        case VisualType::Particles:
            return "Particles";
        case VisualType::Lissajous:
            return "Lissajous";
        case VisualType::FlowField:
            return "FlowField";
        case VisualType::Fractal:
            return "Fractal";
    }
    return "?";
}

const Palette kDebugPaletteWarmCool{
    {{0.0f, Color{40, 90, 255, 255}}, {1.0f, Color{255, 60, 180, 255}}}};
const Palette kDebugPaletteSunset{{{0.0f, Color{255, 220, 0, 255}},
                                    {0.5f, Color{255, 80, 0, 255}},
                                    {1.0f, Color{120, 0, 200, 255}}}};

std::optional<Palette> next_debug_palette(const std::optional<Palette>& current) {
    if (!current.has_value()) {
        return kDebugPaletteWarmCool;
    }
    if (*current == kDebugPaletteWarmCool) {
        return kDebugPaletteSunset;
    }
    return std::nullopt;
}

const char* reactivity_source_name(ReactivitySource source) {
    switch (source) {
        case ReactivitySource::GlobalBeat:
            return "GlobalBeat";
        case ReactivitySource::BandOnset:
            return "BandOnset";
        case ReactivitySource::BandAmplitude:
            return "BandAmplitude";
    }
    return "?";
}

LayerBlendMode next_blend_mode(LayerBlendMode mode) {
    switch (mode) {
        case LayerBlendMode::Normal:
            return LayerBlendMode::Additive;
        case LayerBlendMode::Additive:
            return LayerBlendMode::Multiply;
        case LayerBlendMode::Multiply:
            return LayerBlendMode::Screen;
        case LayerBlendMode::Screen:
            return LayerBlendMode::Normal;
    }
    return LayerBlendMode::Normal;
}

std::string format_layer_cap_error(const Compositor& compositor) {
    char line[128];
    std::snprintf(line, sizeof(line), "ERROR: layer cap reached (%zu/%zu) -- cannot add or duplicate\n",
                  compositor.layers().size(), Compositor::kMaxLayers);
    return line;
}

std::string format_layers_block(const Compositor& compositor, std::size_t selected) {
    std::string block;
    char line[160];
    const std::vector<Layer>& layers = compositor.layers();
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        std::string band_str =
            layer.freq_band.has_value() ? std::to_string(*layer.freq_band) : std::string("Global");
        std::snprintf(line, sizeof(line),
                      "[%c] %zu: %-9s  band=%-6s  reactivity=%-13s  sensitivity=%.1f  opacity=%.1f  "
                      "blend=%-8s%s\n",
                      i == selected ? '*' : ' ', i, visual_type_name(layer.visual_type), band_str.c_str(),
                      reactivity_source_name(layer.reactivity_source), layer.sensitivity, layer.opacity,
                      blend_mode_name(layer.blend_mode), layer.enabled ? "" : "  [hidden]");
        block += line;
    }
    block += "--------------------------------------------------------------\n";
    return block;
}

void refresh_display(const std::string& block, int& last_display_lines) {
    if (last_display_lines > 0) {
        std::printf("\x1b[%dA\x1b[J", last_display_lines);
    }
    std::fputs(block.c_str(), stdout);
    std::fflush(stdout);
    last_display_lines = static_cast<int>(std::count(block.begin(), block.end(), '\n'));
}

std::mt19937 make_seeded_rng() { return std::mt19937(std::random_device{}()); }

void populate_startup_layers(Compositor& compositor, std::optional<int> generate_layer_count) {
    if (!generate_layer_count.has_value()) {
        compositor.add_layer(VisualType::Waveform);
        return;
    }

    std::size_t requested = static_cast<std::size_t>(*generate_layer_count);
    std::size_t effective = std::min(requested, Compositor::kMaxLayers);
    if (effective < requested) {
        std::printf("note: requested %zu layers exceeds cap (%zu) -- generating %zu instead\n", requested,
                    Compositor::kMaxLayers, effective);
    }

    std::mt19937 rng = make_seeded_rng();
    std::vector<GeneratedLayerSpec> specs = generate_layer_specs(effective, rng);
    for (const GeneratedLayerSpec& spec : specs) {
        std::optional<std::size_t> idx = compositor.add_layer(spec.visual_type);
        if (!idx.has_value()) {
            break;
        }
        Layer& layer = compositor.layers()[*idx];
        layer.freq_band = spec.freq_band;
        layer.reactivity_source = spec.reactivity_source;
    }
}
}

int run_app(const char* audio_path, std::optional<int> generate_layer_count) {
    try {
        const std::string original_audio_path = audio_path;

        auto initial_producer = std::make_unique<FileProducer>(original_audio_path);
        std::printf("playing %s (%u ch, %u Hz)\n", audio_path, initial_producer->native_channels(),
                    initial_producer->native_sample_rate());

        AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
        AudioFeaturesSnapshot snapshot;
        FeatureWorker worker(ring, snapshot, audio_config::kPipelineSampleRate);

        worker.start();
        ProducerHost producer_host(ring, std::move(initial_producer));
        AudioSourceController audio_source_controller(producer_host, original_audio_path);

#ifdef AVIZ_VERBOSE_RAYLIB_LOG
        SetTraceLogLevel(LOG_INFO);
#else
        SetTraceLogLevel(LOG_WARNING);
#endif
        InitWindow(window_layout::kVisualizerWidth, window_layout::kVisualizerHeight, "aviz");
        SetTargetFPS(kTargetFps);

        const DockedLayout initial_layout = window_layout::compute_docked_layout(
            GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor()));
        SetWindowPosition(initial_layout.visualizer.x, initial_layout.visualizer.y);

        std::unique_ptr<UIHost> ui_host =
            create_ui_host(resolve_absolute_panel_content_dir(), initial_layout.panel.x, initial_layout.panel.y,
                            initial_layout.panel.width, initial_layout.panel.height);

        std::printf(
            "debug compositor controls (independent of the control panel window -- see "
            "milestone-4-debug-controls.md):\n"
            "  F11       toggle fullscreen (hides/restores the control panel window; syncs panel)\n"
            "  K         toggle docked/detached window layout (syncs panel)\n"
            "  A         add a waveform layer (default freq_band/reactivity/enabled)\n"
            "  1/2/3/4/5/6/7 set selected layer's visual type to "
            "waveform/spectrum/radial/particles/lissajous/flowfield/fractal\n"
            "  D         duplicate selected layer\n"
            "  X/Delete  remove selected layer\n"
            "  up/down   change selected layer\n"
            "  left/right  adjust selected layer's opacity\n"
            "  B         cycle selected layer's blend mode\n"
            "  ,/.       cycle selected layer's freq_band (bands 0-15, Global)\n"
            "  [/]       cycle selected layer's reactivity_source\n"
            "  -/=       adjust selected layer's sensitivity\n"
            "  m         toggle selected layer's enabled state\n"
            "  r/t       adjust selected Radial layer's inner_radius_fraction (no-op on other types)\n"
            "  y/u       adjust selected Radial layer's max_radius_fraction (outer radius, no-op on other types)\n"
            "  f/g       adjust selected Particles layer's inner_radius_fraction (no-op on other types)\n"
            "  h         toggle selected FlowField layer's onset ripple (no-op on other types)\n"
            "  j         toggle selected FlowField layer's burst-on-onset (no-op on other types)\n"
            "  p         cycle selected layer's palette override (off/warm-cool/sunset)\n"
            "  L         toggle audio source between file playback and loopback capture (syncs panel)\n"
            "  S         save the layer stack to presets/%s\n"
            "  O         load the layer stack from presets/%s\n",
            kDebugPresetFilename, kDebugPresetFilename);

        {
            Compositor compositor;
            compositor.resize(initial_layout.visualizer.width, initial_layout.visualizer.height);
            WindowLayoutController window_layout_controller(compositor, *ui_host);
            RaylibWindowWatcher raylib_window_watcher(GetWindowHandle(), [&]() {
                window_layout_controller.set_docked(false);
                panel_message_apply::push_window_state(*ui_host, window_layout_controller,
                                                        audio_source_controller);
            });
            populate_startup_layers(compositor, generate_layer_count);
            std::size_t selected = 0;
            compositor_math::reassign_layer_tints(compositor.layers());
            int last_display_lines = 0;
            refresh_display(format_layers_block(compositor, selected), last_display_lines);

            panel_message_apply::PanelApplyContext apply_ctx{compositor, *ui_host, window_layout_controller,
                                                               audio_source_controller};

            while (!WindowShouldClose()) {
                std::vector<Layer>& layers = compositor.layers();

                if (IsKeyPressed(KEY_A)) {
                    std::string block;
                    if (!compositor.add_layer(VisualType::Waveform).has_value()) {
                        block = format_layer_cap_error(compositor);
                    } else {
                        compositor_math::reassign_layer_tints(compositor.layers());
                    }
                    block += format_layers_block(compositor, selected);
                    refresh_display(block, last_display_lines);
                } else if (IsKeyPressed(KEY_D) && !layers.empty()) {
                    std::string block;
                    if (!compositor.duplicate_layer(selected).has_value()) {
                        block = format_layer_cap_error(compositor);
                    } else {
                        compositor_math::reassign_layer_tints(compositor.layers());
                    }
                    block += format_layers_block(compositor, selected);
                    refresh_display(block, last_display_lines);
                } else if (IsKeyPressed(KEY_ONE) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Waveform);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_TWO) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Spectrum);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_THREE) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Radial);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_FOUR) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Particles);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_FIVE) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Lissajous);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_SIX) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::FlowField);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_SEVEN) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Fractal);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if ((IsKeyPressed(KEY_X) || IsKeyPressed(KEY_DELETE)) && !layers.empty()) {
                    compositor.remove_layer(selected);
                    selected = layers.empty() ? 0 : std::min(selected, layers.size() - 1);
                    compositor_math::reassign_layer_tints(compositor.layers());
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_UP) && selected > 0) {
                    --selected;
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_DOWN) && !layers.empty() && selected + 1 < layers.size()) {
                    ++selected;
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_LEFT) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.opacity = compositor_math::clamp_opacity(layer.opacity - 0.1f);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_RIGHT) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.opacity = compositor_math::clamp_opacity(layer.opacity + 0.1f);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_B) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.blend_mode = next_blend_mode(layer.blend_mode);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_COMMA) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.freq_band =
                        compositor_math::prev_freq_band(layer.freq_band, audio_config::kNumBands);
                    layer.reactivity_source = compositor_math::default_reactivity_source(layer.freq_band);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_PERIOD) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.freq_band =
                        compositor_math::next_freq_band(layer.freq_band, audio_config::kNumBands);
                    layer.reactivity_source = compositor_math::default_reactivity_source(layer.freq_band);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_LEFT_BRACKET) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.reactivity_source =
                        compositor_math::prev_reactivity_source(layer.reactivity_source, layer.freq_band);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_RIGHT_BRACKET) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.reactivity_source =
                        compositor_math::next_reactivity_source(layer.reactivity_source, layer.freq_band);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_MINUS) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.sensitivity = compositor_math::clamp_sensitivity(layer.sensitivity - 0.1f);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_EQUAL) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.sensitivity = compositor_math::clamp_sensitivity(layer.sensitivity + 0.1f);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_M) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.enabled = !layer.enabled;
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_R) && !layers.empty()) {
                    if (auto* radial = dynamic_cast<RadialVisual*>(layers[selected].visual.get())) {
                        RadialParams& p = radial->mutable_params();
                        p.inner_radius_fraction = radial_math::clamp_inner_radius_fraction(
                            p.inner_radius_fraction + 0.05f, p.max_radius_fraction);
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_T) && !layers.empty()) {
                    if (auto* radial = dynamic_cast<RadialVisual*>(layers[selected].visual.get())) {
                        RadialParams& p = radial->mutable_params();
                        p.inner_radius_fraction = radial_math::clamp_inner_radius_fraction(
                            p.inner_radius_fraction - 0.05f, p.max_radius_fraction);
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_Y) && !layers.empty()) {
                    if (auto* radial = dynamic_cast<RadialVisual*>(layers[selected].visual.get())) {
                        RadialParams& p = radial->mutable_params();
                        p.max_radius_fraction = radial_math::clamp_max_radius_fraction(
                            p.max_radius_fraction + 0.05f, p.inner_radius_fraction);
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_U) && !layers.empty()) {
                    if (auto* radial = dynamic_cast<RadialVisual*>(layers[selected].visual.get())) {
                        RadialParams& p = radial->mutable_params();
                        p.max_radius_fraction = radial_math::clamp_max_radius_fraction(
                            p.max_radius_fraction - 0.05f, p.inner_radius_fraction);
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_F) && !layers.empty()) {
                    if (auto* particles = dynamic_cast<ParticlesVisual*>(layers[selected].visual.get())) {
                        ParticlesParams& p = particles->mutable_params();
                        p.inner_radius_fraction =
                            particles_math::clamp_inner_radius_fraction(p.inner_radius_fraction + 0.05f);
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_G) && !layers.empty()) {
                    if (auto* particles = dynamic_cast<ParticlesVisual*>(layers[selected].visual.get())) {
                        ParticlesParams& p = particles->mutable_params();
                        p.inner_radius_fraction =
                            particles_math::clamp_inner_radius_fraction(p.inner_radius_fraction - 0.05f);
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_H) && !layers.empty()) {
                    if (auto* flow_field = dynamic_cast<FlowFieldVisual*>(layers[selected].visual.get())) {
                        FlowFieldParams& p = flow_field->mutable_params();
                        p.ripple_on_onset_enabled = !p.ripple_on_onset_enabled;
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_J) && !layers.empty()) {
                    if (auto* flow_field = dynamic_cast<FlowFieldVisual*>(layers[selected].visual.get())) {
                        FlowFieldParams& p = flow_field->mutable_params();
                        p.burst_on_onset_enabled = !p.burst_on_onset_enabled;
                    }
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_P) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.palette_override = next_debug_palette(layer.palette_override);
                    refresh_display(format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_S)) {
                    const std::string path = preset_file_io::preset_path(kDebugPresetFilename);
                    const std::string json_text = layer_preset::serialize(compositor.layers());
                    preset_file_io::WriteResult result = preset_file_io::write_file(path, json_text);
                    std::string status = result.success
                                              ? "saved " + std::to_string(compositor.layers().size()) +
                                                    " layer(s) to " + path + "\n"
                                              : "ERROR: failed saving preset to " + path + ": " + result.error + "\n";
                    refresh_display(status + format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_O)) {
                    const std::string path = preset_file_io::preset_path(kDebugPresetFilename);
                    preset_file_io::ReadResult read_result = preset_file_io::read_file(path);
                    std::string status;
                    if (!read_result.success) {
                        status = "ERROR: failed loading preset from " + path + ": " + read_result.error + "\n";
                    } else {
                        std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(read_result.contents);
                        if (!loaded.has_value()) {
                            status = "ERROR: preset at " + path + " is malformed or invalid\n";
                        } else if (!compositor.replace_layers_if_ids_valid(std::move(*loaded))) {
                            status = "ERROR: preset at " + path + " was rejected by the compositor\n";
                        } else {
                            selected = compositor.layers().empty()
                                           ? 0
                                           : std::min(selected, compositor.layers().size() - 1);
                            status = "loaded " + std::to_string(compositor.layers().size()) + " layer(s) from " +
                                      path + "\n";
                        }
                    }
                    refresh_display(status + format_layers_block(compositor, selected), last_display_lines);
                } else if (IsKeyPressed(KEY_L)) {
                    AudioSourceController::SwitchResult result = audio_source_controller.toggle();
                    if (result.success) {
                        std::printf("switched to %s (%u ch, %u Hz)\n",
                                    audio_source_controller.is_loopback() ? "loopback capture" : "file playback",
                                    result.channels, result.sample_rate);
                    } else {
                        std::fprintf(stderr, "error switching audio source: %s\n", result.error.c_str());
                    }
                    panel_message_apply::push_window_state(*ui_host, window_layout_controller,
                                                            audio_source_controller);
                } else if (IsKeyPressed(KEY_F11)) {
                    window_layout_controller.toggle_fullscreen();
                    panel_message_apply::push_window_state(*ui_host, window_layout_controller,
                                                            audio_source_controller);
                } else if (IsKeyPressed(KEY_K)) {
                    window_layout_controller.toggle_dock();
                    panel_message_apply::push_window_state(*ui_host, window_layout_controller,
                                                            audio_source_controller);
                }

                for (const PanelMessage& message : ui_host->drain_pending_messages()) {
                    panel_message_apply::apply(message, apply_ctx);
                }

                compositor.render(snapshot.current());

                BeginDrawing();
                ClearBackground(BLACK);
                RenderTexture2D& tex = compositor.output();
                DrawTextureRec(
                    tex.texture,
                    Rectangle{0.0f, 0.0f, static_cast<float>(tex.texture.width),
                              -static_cast<float>(tex.texture.height)},
                    Vector2{0.0f, 0.0f}, WHITE);
                EndDrawing();
            }
        }

        CloseWindow();

        worker.stop();
        std::printf("done\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
