#include "visuals/fractal/fractal_visual.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "audio/audio_config.h"
#include "rlgl.h"
#include "visuals/number_clamp.h"

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

float apply_sensitivity(float amplitude01, float sensitivity) {
    return std::clamp(amplitude01 * sensitivity, 0.0f, 1.0f);
}

float amplitude_to_zoom(float amplitude01, float base_zoom, float zoom_amplitude_scale) {
    const float clamped = std::clamp(amplitude01, 0.0f, 1.0f);
    return base_zoom * (1.0f + clamped * zoom_amplitude_scale);
}

float advance_orbit_phase(float phase_radians, float angular_speed_radians_per_sec, bool clockwise,
                           float dt_seconds) {
    const float direction = clockwise ? 1.0f : -1.0f;
    return std::fmod(phase_radians + angular_speed_radians_per_sec * direction * dt_seconds, 2.0f * PI);
}

Vector2 julia_constant(float orbit_phase_radians, float amplitude01, float base_radius,
                        float radius_amplitude_scale, Vector2 orbit_center) {
    const float clamped = std::clamp(amplitude01, 0.0f, 1.0f);
    const float radius = base_radius + clamped * radius_amplitude_scale;
    return Vector2{orbit_center.x + radius * std::cos(orbit_phase_radians),
                   orbit_center.y + radius * std::sin(orbit_phase_radians)};
}

float advance_hue_phase(float phase01, float cycle_speed_hz, float dt_seconds) {
    const float raw = phase01 + cycle_speed_hz * dt_seconds;
    return raw - std::floor(raw);
}

float clamp_base_zoom(double zoom) {
    return number_math::clamp_number_to_range<float>(zoom, visual_config::kFractalBaseZoomMin,
                                                      visual_config::kFractalBaseZoomMax);
}

float clamp_zoom_amplitude_scale(double scale) {
    return number_math::clamp_number_to_range<float>(scale, visual_config::kFractalZoomAmplitudeScaleMin,
                                                      visual_config::kFractalZoomAmplitudeScaleMax);
}

float clamp_julia_base_radius(double radius) {
    return number_math::clamp_number_to_range<float>(radius, visual_config::kFractalJuliaBaseRadiusMin,
                                                      visual_config::kFractalJuliaBaseRadiusMax);
}

float clamp_julia_radius_amplitude_scale(double scale) {
    return number_math::clamp_number_to_range<float>(scale, visual_config::kFractalJuliaRadiusAmplitudeScaleMin,
                                                      visual_config::kFractalJuliaRadiusAmplitudeScaleMax);
}

float clamp_julia_angular_speed(double radians_per_sec) {
    return number_math::clamp_number_to_range<float>(radians_per_sec,
                                                      visual_config::kFractalJuliaAngularSpeedRadiansPerSecMin,
                                                      visual_config::kFractalJuliaAngularSpeedRadiansPerSecMax);
}

float clamp_julia_orbit_center(double coordinate) {
    return number_math::clamp_number_to_range<float>(coordinate, visual_config::kFractalJuliaOrbitCenterMin,
                                                      visual_config::kFractalJuliaOrbitCenterMax);
}

float clamp_color_cycle_speed_hz(double hz) {
    return number_math::clamp_number_to_range<float>(hz, visual_config::kFractalColorCycleSpeedHzMin,
                                                      visual_config::kFractalColorCycleSpeedHzMax);
}

}

void FractalVisual::sync_palette_texture(const std::optional<Palette>& palette_override) {
    if (palette_override == cached_palette_) {
        return;
    }
    if (has_palette_texture_) {
        UnloadTexture(palette_texture_);
        has_palette_texture_ = false;
    }
    if (palette_override.has_value()) {
        const int width = visual_config::kFractalPaletteGradientWidth;
        std::vector<Color> pixels = palette_math::to_gradient_pixels(*palette_override, width);
        Image image{pixels.data(), width, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        palette_texture_ = LoadTextureFromImage(image);
        has_palette_texture_ = true;
    }
    cached_palette_ = palette_override;
}

FractalVisual::FractalVisual(FractalParams params) : FractalVisual(params, load_shared_shader()) {}

FractalVisual::FractalVisual(FractalParams params, std::shared_ptr<Shader> shared_shader)
    : params_(params), shader_(std::move(shared_shader)) {
    shader_ready_ = shader_->id != 0 && shader_->id != rlGetShaderIdDefault();
    if (shader_ready_) {
        cache_uniform_locations();
    }
}

void FractalVisual::cache_uniform_locations() {
    loc_resolution_ = GetShaderLocation(*shader_, "resolution");
    loc_zoom_ = GetShaderLocation(*shader_, "zoom");
    loc_julia_c_ = GetShaderLocation(*shader_, "juliaC");
    loc_hue_shift_ = GetShaderLocation(*shader_, "hueShift");
    loc_max_iterations_ = GetShaderLocation(*shader_, "maxIterations");
    loc_boundary_margin_px_ = GetShaderLocation(*shader_, "boundaryMarginPx");
    loc_palette_tex_ = GetShaderLocation(*shader_, "paletteTex");
    loc_use_palette_texture_ = GetShaderLocation(*shader_, "usePaletteTexture");
}

FractalVisual::~FractalVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
        UnloadTexture(canvas_texture_);
    }
    if (has_palette_texture_) {
        UnloadTexture(palette_texture_);
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

void FractalVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity,
                            const std::optional<Palette>& palette_override) {
    if (!shader_ready_) {
        BeginTextureMode(texture_);
        ClearBackground(BLANK);
        EndTextureMode();
        return;
    }

    const float amplitude = fractal_math::selected_band_amplitude(
        features.band_energy, features.band_agc_reference, reactivity.freq_band);

    const float scaled_amplitude = fractal_math::apply_sensitivity(amplitude, reactivity.sensitivity);

    const float dt = GetFrameTime();
    smoothed_amplitude_ = fractal_math::one_pole_smooth(smoothed_amplitude_, scaled_amplitude,
                                                         params_.amplitude_smoothing_tau_ms, dt);
    orbit_phase_radians_ = fractal_math::advance_orbit_phase(
        orbit_phase_radians_, params_.julia_angular_speed_radians_per_sec, params_.julia_orbit_clockwise, dt);
    hue_phase_ = fractal_math::advance_hue_phase(hue_phase_, params_.color_cycle_speed_hz, dt);

    const float zoom = fractal_math::amplitude_to_zoom(smoothed_amplitude_, params_.base_zoom,
                                                          params_.zoom_amplitude_scale);
    const Vector2 julia_c = fractal_math::julia_constant(
        orbit_phase_radians_, smoothed_amplitude_, params_.julia_base_radius,
        params_.julia_radius_amplitude_scale,
        Vector2{params_.julia_orbit_center_x, params_.julia_orbit_center_y});

    sync_palette_texture(palette_override);

    const float resolution[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    const float julia_c_uniform[2] = {julia_c.x, julia_c.y};
    const int max_iterations = visual_config::kFractalMaxIterations;
    const int use_palette_texture = has_palette_texture_ ? 1 : 0;

    SetShaderValue(*shader_, loc_resolution_, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(*shader_, loc_zoom_, &zoom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(*shader_, loc_julia_c_, julia_c_uniform, SHADER_UNIFORM_VEC2);
    SetShaderValue(*shader_, loc_hue_shift_, &hue_phase_, SHADER_UNIFORM_FLOAT);
    SetShaderValue(*shader_, loc_max_iterations_, &max_iterations, SHADER_UNIFORM_INT);
    SetShaderValue(*shader_, loc_boundary_margin_px_, &params_.boundary_margin_px, SHADER_UNIFORM_FLOAT);
    SetShaderValue(*shader_, loc_use_palette_texture_, &use_palette_texture, SHADER_UNIFORM_INT);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    BeginShaderMode(*shader_);
    if (has_palette_texture_) {
        SetShaderValueTexture(*shader_, loc_palette_tex_, palette_texture_);
    }
    DrawTexture(canvas_texture_, 0, 0, WHITE);
    EndShaderMode();
    EndTextureMode();
}
