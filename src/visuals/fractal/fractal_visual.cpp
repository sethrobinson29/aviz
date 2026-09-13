#include "visuals/fractal/fractal_visual.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "audio/audio_config.h"

namespace {

std::string resolve_shader_path() {
    const char* app_dir = GetApplicationDirectory();
    std::string candidate = std::string(app_dir) + "assets/shaders/mandelbrot.fs";
    if (FileExists(candidate.c_str())) {
        return candidate;
    }
    return "assets/shaders/mandelbrot.fs";
}

std::shared_ptr<Shader> load_shared_shader() {
    return std::shared_ptr<Shader>(new Shader(LoadShader(nullptr, resolve_shader_path().c_str())),
                                    [](Shader* shader) {
                                        if (shader->id != 0) {
                                            UnloadShader(*shader);
                                        }
                                        delete shader;
                                    });
}

}

namespace fractal_math {

float selected_band_amplitude(const std::array<float, audio_config::kTotalBandSlots>& band_energy,
                               const std::array<float, audio_config::kTotalBandSlots>& band_reference,
                               std::optional<std::size_t> freq_band) {
    const std::size_t index = freq_band.value_or(audio_config::kGlobalBandIndex);
    if (index >= audio_config::kTotalBandSlots || band_reference[index] <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(band_energy[index] / band_reference[index], 0.0f, 1.0f);
}

float one_pole_smooth(float current, float target, float time_constant_ms, float dt_seconds) {
    if (time_constant_ms <= 0.0f) {
        return target;
    }
    const float tau = time_constant_ms / 1000.0f;
    const float coeff = 1.0f - std::exp(-dt_seconds / tau);
    return current + (target - current) * coeff;
}

float amplitude_to_zoom(float amplitude01, float base_zoom, float zoom_amplitude_scale) {
    const float clamped = std::clamp(amplitude01, 0.0f, 1.0f);
    return base_zoom * (1.0f + clamped * zoom_amplitude_scale);
}

Vector2 julia_constant(float time_seconds, float amplitude01, float base_radius,
                        float radius_amplitude_scale, float angular_speed_radians_per_sec,
                        Vector2 orbit_center, bool clockwise) {
    const float clamped = std::clamp(amplitude01, 0.0f, 1.0f);
    const float radius = base_radius + clamped * radius_amplitude_scale;
    const float direction = clockwise ? 1.0f : -1.0f;
    const float angle = time_seconds * angular_speed_radians_per_sec * direction;
    return Vector2{orbit_center.x + radius * std::cos(angle), orbit_center.y + radius * std::sin(angle)};
}

float color_cycle_hue(float time_seconds, float cycle_speed_hz) {
    const float raw = time_seconds * cycle_speed_hz;
    return raw - std::floor(raw);
}

}

FractalVisual::FractalVisual(FractalParams params) : FractalVisual(params, load_shared_shader()) {}

FractalVisual::FractalVisual(FractalParams params, std::shared_ptr<Shader> shared_shader)
    : params_(params), shader_(std::move(shared_shader)) {
    cache_uniform_locations();
}

void FractalVisual::cache_uniform_locations() {
    loc_resolution_ = GetShaderLocation(*shader_, "resolution");
    loc_zoom_ = GetShaderLocation(*shader_, "zoom");
    loc_julia_c_ = GetShaderLocation(*shader_, "juliaC");
    loc_hue_shift_ = GetShaderLocation(*shader_, "hueShift");
    loc_max_iterations_ = GetShaderLocation(*shader_, "maxIterations");
    loc_boundary_margin_px_ = GetShaderLocation(*shader_, "boundaryMarginPx");
}

FractalVisual::~FractalVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
        UnloadTexture(canvas_texture_);
    }
}

std::unique_ptr<VisualModule> FractalVisual::clone() const {
    return std::unique_ptr<FractalVisual>(new FractalVisual(params_, shader_));
}

void FractalVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
        UnloadTexture(canvas_texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    Image blank = GenImageColor(width, height, WHITE);
    canvas_texture_ = LoadTextureFromImage(blank);
    UnloadImage(blank);
    width_ = width;
    height_ = height;
}

void FractalVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity) {
    const float amplitude = fractal_math::selected_band_amplitude(
        features.band_energy, features.band_agc_reference, reactivity.freq_band);

    const float dt = GetFrameTime();
    elapsed_seconds_ += dt;
    smoothed_amplitude_ =
        fractal_math::one_pole_smooth(smoothed_amplitude_, amplitude, params_.amplitude_smoothing_tau_ms, dt);

    const float zoom = fractal_math::amplitude_to_zoom(smoothed_amplitude_, params_.base_zoom,
                                                          params_.zoom_amplitude_scale);
    const Vector2 julia_c = fractal_math::julia_constant(
        elapsed_seconds_, smoothed_amplitude_, params_.julia_base_radius,
        params_.julia_radius_amplitude_scale, params_.julia_angular_speed_radians_per_sec,
        Vector2{params_.julia_orbit_center_x, params_.julia_orbit_center_y}, params_.julia_orbit_clockwise);
    const float hue = fractal_math::color_cycle_hue(elapsed_seconds_, params_.color_cycle_speed_hz);

    const float resolution[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    const float julia_c_uniform[2] = {julia_c.x, julia_c.y};
    const int max_iterations = visual_config::kFractalMaxIterations;

    SetShaderValue(*shader_, loc_resolution_, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(*shader_, loc_zoom_, &zoom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(*shader_, loc_julia_c_, julia_c_uniform, SHADER_UNIFORM_VEC2);
    SetShaderValue(*shader_, loc_hue_shift_, &hue, SHADER_UNIFORM_FLOAT);
    SetShaderValue(*shader_, loc_max_iterations_, &max_iterations, SHADER_UNIFORM_INT);
    SetShaderValue(*shader_, loc_boundary_margin_px_, &params_.boundary_margin_px, SHADER_UNIFORM_FLOAT);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    BeginShaderMode(*shader_);
    DrawTexture(canvas_texture_, 0, 0, WHITE);
    EndShaderMode();
    EndTextureMode();
}
