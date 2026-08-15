#include "app/app.h"

#include <cstdio>
#include <stdexcept>

#include "audio/audio_config.h"
#include "audio/audio_features.h"
#include "audio/feature_worker.h"
#include "audio/file_producer.h"
#include "audio/ring_buffer.h"
#include "raylib.h"
#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/visual_module.h"
#include "visuals/waveform/waveform_visual.h"

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
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

        {
            // Scoped so textures unload before CloseWindow() tears down the
            // GL context.
            WaveformVisual waveform;
            SpectrumVisual spectrum;
            waveform.resize(kWindowWidth, kWindowHeight);
            spectrum.resize(kWindowWidth, kWindowHeight);

            VisualModule* active = &waveform;

            // Loop runs on window lifetime, not producer.is_finished() --
            // a finished track keeps playing silence, so visuals settle
            // into their idle pose instead of the window closing.
            while (!WindowShouldClose()) {
                if (IsKeyPressed(KEY_ONE)) {
                    active = &waveform;
                } else if (IsKeyPressed(KEY_TWO)) {
                    active = &spectrum;
                }

                active->render(snapshot.current());

                BeginDrawing();
                ClearBackground(BLACK);
                RenderTexture2D& tex = active->render_texture();
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
