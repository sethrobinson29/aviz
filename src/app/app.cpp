#include "app/app.h"

#include <cstdio>
#include <stdexcept>

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

const char* blend_mode_name(LayerBlendMode mode) {
    switch (mode) {
        case LayerBlendMode::Normal:
            return "normal";
        case LayerBlendMode::Additive:
            return "additive";
        case LayerBlendMode::Multiply:
            return "multiply";
        case LayerBlendMode::Screen:
            return "screen";
    }
    return "?";
}

const char* visual_type_name(VisualType type) {
    switch (type) {
        case VisualType::Waveform:
            return "waveform";
        case VisualType::Spectrum:
            return "spectrum";
    }
    return "?";
}

void print_stack(Compositor& compositor) {
    std::printf("stack (bottom -> top):");
    for (const Layer& layer : compositor.layers()) {
        std::printf(" %s", visual_type_name(layer.visual_type));
    }
    std::printf("\n");
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

        InitWindow(kWindowWidth, kWindowHeight, "aviz");
        SetTargetFPS(kTargetFps);

        std::printf(
            "compositor controls (no control panel UI yet -- milestone 8):\n"
            "  1/2       add waveform/spectrum layer to top of stack\n"
            "  X         remove top layer\n"
            "  B         cycle top layer's blend mode\n"
            "  down/up   decrease/increase top layer's opacity\n"
            "  O         move top layer to bottom of stack\n");

        {
            Compositor compositor;
            compositor.resize(kWindowWidth, kWindowHeight);
            compositor.add_layer(VisualType::Waveform);

            while (!WindowShouldClose()) {
                if (IsKeyPressed(KEY_ONE)) {
                    compositor.add_layer(VisualType::Waveform);
                    print_stack(compositor);
                } else if (IsKeyPressed(KEY_TWO)) {
                    compositor.add_layer(VisualType::Spectrum);
                    print_stack(compositor);
                } else if (IsKeyPressed(KEY_X) && !compositor.layers().empty()) {
                    compositor.remove_layer(compositor.layers().size() - 1);
                    print_stack(compositor);
                } else if (IsKeyPressed(KEY_B) && !compositor.layers().empty()) {
                    Layer& top = compositor.layers().back();
                    top.blend_mode = next_blend_mode(top.blend_mode);
                    std::printf("top layer blend mode: %s\n", blend_mode_name(top.blend_mode));
                } else if (IsKeyPressed(KEY_DOWN) && !compositor.layers().empty()) {
                    Layer& top = compositor.layers().back();
                    top.opacity = compositor_math::clamp_opacity(top.opacity - 0.1f);
                    std::printf("top layer opacity: %.1f\n", top.opacity);
                } else if (IsKeyPressed(KEY_UP) && !compositor.layers().empty()) {
                    Layer& top = compositor.layers().back();
                    top.opacity = compositor_math::clamp_opacity(top.opacity + 0.1f);
                    std::printf("top layer opacity: %.1f\n", top.opacity);
                } else if (IsKeyPressed(KEY_O) && compositor.layers().size() > 1) {
                    compositor.move_layer(compositor.layers().size() - 1, 0);
                    print_stack(compositor);
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
