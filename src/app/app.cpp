#include "app/app.h"

#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
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
#include "audio/producer_host.h"
#include "audio/ring_buffer.h"
#include "compositor/compositor.h"
#include "compositor/compositor_math.h"
#include "compositor/layer.h"
#include "compositor/preset_file_io.h"
#include "raylib.h"
#include "ui_host/panel_message_apply.h"
#include "ui_host/ui_host.h"

namespace {
constexpr int kTargetFps = 60;

std::string resolve_absolute_panel_content_dir() {
    const char* app_dir = GetApplicationDirectory();
    std::string candidate = std::string(app_dir) + "web/panel";
    std::string resolved = DirectoryExists(candidate.c_str()) ? candidate : std::string("web/panel");
    return std::filesystem::absolute(resolved).u8string();
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

int run_app(std::optional<int> generate_layer_count) {
    try {
        AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
        AudioFeaturesSnapshot snapshot;
        FeatureWorker worker(ring, snapshot, audio_config::kPipelineSampleRate);

        worker.start();
        ProducerHost producer_host(ring);
        AudioSourceController audio_source_controller(producer_host);

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
            "hotkeys:\n"
            "  F11       toggle fullscreen (hides/restores the control panel window; syncs panel)\n"
            "  K         toggle docked/detached window layout (syncs panel)\n"
            "  L         toggle audio source between file playback and loopback capture (syncs panel)\n");

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
            compositor_math::reassign_layer_tints(compositor.layers());

            const std::string presets_dir = preset_file_io::presets_directory_in(GetApplicationDirectory());
            panel_message_apply::PanelApplyContext apply_ctx{compositor, *ui_host, window_layout_controller,
                                                               audio_source_controller, presets_dir};

            while (!WindowShouldClose() && !ui_host->should_close()) {
                if (IsKeyPressed(KEY_L)) {
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
