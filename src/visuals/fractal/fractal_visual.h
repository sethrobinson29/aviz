#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

#include "audio/audio_config.h"
#include "raylib.h"
#include "visuals/visual_config.h"
#include "visuals/visual_module.h"

namespace fractal_math {

float selected_band_amplitude(const std::array<float, audio_config::kTotalBandSlots>& band_energy,
                               const std::array<float, audio_config::kTotalBandSlots>& band_reference,
                               std::optional<std::size_t> freq_band);

bool selected_band_onset(const std::array<bool, audio_config::kTotalBandSlots>& band_onset,
                          std::optional<std::size_t> freq_band);

float amplitude_to_zoom(float amplitude01, float base_zoom, float zoom_amplitude_scale);

Vector2 julia_constant(float time_seconds, float amplitude01, float base_radius,
                        float radius_amplitude_scale, float angular_speed_radians_per_sec);

float color_cycle_hue(float time_seconds, float cycle_speed_hz, float pulse, float pulse_hue_boost);

}

struct FractalParams {
    float base_zoom = visual_config::kFractalBaseZoom;
    float zoom_amplitude_scale = visual_config::kFractalZoomAmplitudeScale;
    float julia_base_radius = visual_config::kFractalJuliaBaseRadius;
    float julia_radius_amplitude_scale = visual_config::kFractalJuliaRadiusAmplitudeScale;
    float julia_angular_speed_radians_per_sec = visual_config::kFractalJuliaAngularSpeedRadiansPerSec;
    float color_cycle_speed_hz = visual_config::kFractalColorCycleSpeedHz;
    float pulse_hue_boost = visual_config::kFractalPulseHueBoost;
};

class FractalVisual : public VisualModule {
public:
    explicit FractalVisual(FractalParams params = {});
    ~FractalVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override;
    const FractalParams& params() const { return params_; }
    FractalParams& mutable_params() { return params_; }

private:
    FractalVisual(FractalParams params, std::shared_ptr<Shader> shared_shader);

    void cache_uniform_locations();

    FractalParams params_;
    RenderTexture2D texture_{};
    Texture2D canvas_texture_{};
    int width_ = 0;
    int height_ = 0;
    std::shared_ptr<Shader> shader_;
    int loc_resolution_ = -1;
    int loc_zoom_ = -1;
    int loc_julia_c_ = -1;
    int loc_hue_shift_ = -1;
    int loc_pulse_ = -1;
    int loc_max_iterations_ = -1;
    float elapsed_seconds_ = 0.0f;
    std::array<float, audio_config::kTotalBandSlots> onset_pulse_{};
};
