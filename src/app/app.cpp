#include "app/app.h"

#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "audio/audio_config.h"
#include "audio/audio_features.h"
#include "audio/feature_worker.h"
#include "audio/file_producer.h"
#include "audio/ring_buffer.h"
#include "compositor/compositor.h"
#include "compositor/compositor_math.h"
#include "compositor/layer.h"
#include "raylib.h"

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;

// Milestone 4 debug tool palette -- see milestone-4-debug-controls.md.
// clang-format off
constexpr Color kLayerPalette[Compositor::kMaxLayers] = {
    Color{0xFF, 0x3B, 0x30, 0xFF}, Color{0xFF, 0x95, 0x00, 0xFF},
    Color{0xFF, 0xCC, 0x00, 0xFF}, Color{0xA3, 0xD9, 0x00, 0xFF},
    Color{0x34, 0xC7, 0x59, 0xFF}, Color{0x00, 0xC7, 0xBE, 0xFF},
    Color{0x00, 0xA3, 0xFF, 0xFF}, Color{0x00, 0x7A, 0xFF, 0xFF},
    Color{0x58, 0x56, 0xD6, 0xFF}, Color{0xAF, 0x52, 0xDE, 0xFF},
    Color{0xFF, 0x2D, 0x95, 0xFF}, Color{0x8E, 0x8E, 0x93, 0xFF},
    Color{0xC7, 0xA2, 0x6A, 0xFF}, Color{0x7E, 0xD3, 0x21, 0xFF},
    Color{0xFF, 0x6B, 0x6B, 0xFF}, Color{0x4A, 0x90, 0xD9, 0xFF},
};
// clang-format on

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
    }
    return "?";
}

const char* reactivity_source_name(ReactivitySource source) {
    switch (source) {
        case ReactivitySource::GlobalBeat:
            return "GlobalBeat";
        case ReactivitySource::BandOnset:
            return "BandOnset";
        case ReactivitySource::BandAmplitude:
            return "BandAmplitude";
        case ReactivitySource::PeakFreq:
            return "PeakFreq";
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

// Reassigns every layer's tint_color from kLayerPalette by current stack
// position -- called after any add/duplicate/remove, per
// milestone-4-debug-controls.md's Color assignment section.
void reassign_tints(Compositor& compositor) {
    std::vector<Layer>& layers = compositor.layers();
    for (std::size_t i = 0; i < layers.size(); ++i) {
        layers[i].tint_color = kLayerPalette[i];
    }
}

void print_layer_cap_error(const Compositor& compositor) {
    std::printf("ERROR: layer cap reached (%zu/%zu) -- cannot add or duplicate\n",
                compositor.layers().size(), Compositor::kMaxLayers);
}

void print_layers(const Compositor& compositor, std::size_t selected) {
    const std::vector<Layer>& layers = compositor.layers();
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        std::printf("[%c] %zu: %-9s", i == selected ? '*' : ' ', i,
                    visual_type_name(layer.visual_type));
        if (layer.freq_band.has_value()) {
            std::printf("  band=%-6zu", *layer.freq_band);
        } else {
            std::printf("  band=%-6s", "Global");
        }
        std::printf("  reactivity=%-13s  sensitivity=%.1f  opacity=%.1f  blend=%-8s%s\n",
                    reactivity_source_name(layer.reactivity_source), layer.sensitivity, layer.opacity,
                    blend_mode_name(layer.blend_mode), layer.enabled ? "" : "  [hidden]");
    }
    std::printf("--------------------------------------------------------------\n");
}
}  // namespace

int run_app(const char* audio_path) {
    try {
        FileProducer producer(audio_path);
        AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, producer.channels());
        AudioFeaturesSnapshot snapshot;
        FeatureWorker worker(ring, snapshot, producer.sample_rate());

        std::printf("playing %s (%u ch, %u Hz)\n", audio_path, producer.channels(),
                    producer.sample_rate());

        worker.start();
        producer.start(ring);

#ifdef AVIZ_VERBOSE_RAYLIB_LOG
        SetTraceLogLevel(LOG_INFO);
#else
        SetTraceLogLevel(LOG_WARNING);
#endif
        InitWindow(kWindowWidth, kWindowHeight, "aviz");
        SetTargetFPS(kTargetFps);

        std::printf(
            "compositor controls (no control panel UI yet -- milestone 8, see "
            "milestone-4-debug-controls.md):\n"
            "  A         add a waveform layer (default freq_band/reactivity/enabled)\n"
            "  1/2       set selected layer's visual type to waveform/spectrum\n"
            "  D         duplicate selected layer\n"
            "  X/Delete  remove selected layer\n"
            "  up/down   change selected layer\n"
            "  left/right  adjust selected layer's opacity\n"
            "  B         cycle selected layer's blend mode\n"
            "  ,/.       cycle selected layer's freq_band (bands 0-15, Global)\n"
            "  [/]       cycle selected layer's reactivity_source\n"
            "  -/=       adjust selected layer's sensitivity\n"
            "  m         toggle selected layer's enabled state\n");

        {
            Compositor compositor;
            compositor.resize(kWindowWidth, kWindowHeight);
            compositor.add_layer(VisualType::Waveform);
            std::size_t selected = 0;
            reassign_tints(compositor);
            print_layers(compositor, selected);

            while (!WindowShouldClose()) {
                std::vector<Layer>& layers = compositor.layers();

                if (IsKeyPressed(KEY_A)) {
                    if (!compositor.add_layer(VisualType::Waveform).has_value()) {
                        print_layer_cap_error(compositor);
                    } else {
                        reassign_tints(compositor);
                    }
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_D) && !layers.empty()) {
                    if (!compositor.duplicate_layer(selected).has_value()) {
                        print_layer_cap_error(compositor);
                    } else {
                        reassign_tints(compositor);
                    }
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_ONE) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Waveform);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_TWO) && !layers.empty()) {
                    compositor.set_layer_visual_type(selected, VisualType::Spectrum);
                    print_layers(compositor, selected);
                } else if ((IsKeyPressed(KEY_X) || IsKeyPressed(KEY_DELETE)) && !layers.empty()) {
                    compositor.remove_layer(selected);
                    selected = layers.empty() ? 0 : std::min(selected, layers.size() - 1);
                    reassign_tints(compositor);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_UP) && selected > 0) {
                    --selected;
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_DOWN) && !layers.empty() && selected + 1 < layers.size()) {
                    ++selected;
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_LEFT) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.opacity = compositor_math::clamp_opacity(layer.opacity - 0.1f);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_RIGHT) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.opacity = compositor_math::clamp_opacity(layer.opacity + 0.1f);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_B) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.blend_mode = next_blend_mode(layer.blend_mode);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_COMMA) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.freq_band =
                        compositor_math::prev_freq_band(layer.freq_band, audio_config::kNumBands);
                    layer.reactivity_source = compositor_math::default_reactivity_source(layer.freq_band);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_PERIOD) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.freq_band =
                        compositor_math::next_freq_band(layer.freq_band, audio_config::kNumBands);
                    layer.reactivity_source = compositor_math::default_reactivity_source(layer.freq_band);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_LEFT_BRACKET) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.reactivity_source =
                        compositor_math::prev_reactivity_source(layer.reactivity_source, layer.freq_band);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_RIGHT_BRACKET) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.reactivity_source =
                        compositor_math::next_reactivity_source(layer.reactivity_source, layer.freq_band);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_MINUS) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.sensitivity = compositor_math::clamp_sensitivity(layer.sensitivity - 0.1f);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_EQUAL) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.sensitivity = compositor_math::clamp_sensitivity(layer.sensitivity + 0.1f);
                    print_layers(compositor, selected);
                } else if (IsKeyPressed(KEY_M) && !layers.empty()) {
                    Layer& layer = layers[selected];
                    layer.enabled = !layer.enabled;
                    print_layers(compositor, selected);
                }

                compositor.render(snapshot.current());

                BeginDrawing();
                ClearBackground(BLACK);
                RenderTexture2D& tex = compositor.output();
                // Negate height: offscreen textures are Y-flipped vs. screen.
                DrawTextureRec(
                    tex.texture,
                    Rectangle{0.0f, 0.0f, static_cast<float>(tex.texture.width),
                              -static_cast<float>(tex.texture.height)},
                    Vector2{0.0f, 0.0f}, WHITE);
                EndDrawing();
            }
        }

        CloseWindow();

        producer.stop();
        worker.stop();
        std::printf("done\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
