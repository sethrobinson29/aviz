#include "visuals/lissajous/lissajous_visual.h"

#include <cmath>

#include "visuals/visual_config.h"
#include "visuals/waveform/waveform_visual.h"

namespace lissajous_math {

float compress_sample(float sample, float exponent) {
    return std::copysign(std::pow(std::fabs(sample), exponent), sample);
}

std::vector<Vector2> to_points(const float* left, const float* right, std::size_t count,
                                int width, int height, bool idle, float amplitude_scale,
                                float compression_exponent) {
    const float mid_x = static_cast<float>(width) * 0.5f;
    const float mid_y = static_cast<float>(height) * 0.5f;

    if (idle || count == 0) {
        return {Vector2{mid_x, mid_y}, Vector2{mid_x, mid_y}};
    }

    std::vector<Vector2> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float compressed_left = compress_sample(left[i], compression_exponent);
        const float compressed_right = compress_sample(right[i], compression_exponent);
        const float x = mid_x + compressed_left * amplitude_scale * mid_x;
        const float y = mid_y - compressed_right * amplitude_scale * mid_y;
        points.push_back(Vector2{x, y});
    }
    return points;
}

}

LissajousVisual::LissajousVisual(LissajousParams params) : params_(params) {}

LissajousVisual::~LissajousVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
}

void LissajousVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void LissajousVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity,
                              const std::optional<Palette>& palette_override) {
    const std::size_t idle_band = waveform_math::safe_band_index(reactivity.freq_band);
    const bool idle = features.band_idle[idle_band];

    const waveform_math::ReactivityResult reactivity_result =
        waveform_math::resolve_reactivity(features, reactivity);
    const bool band_changed = last_freq_band_ != reactivity.freq_band;
    onset_pulse_ = waveform_math::next_onset_pulse(
        onset_pulse_, band_changed, reactivity_result.onset_active,
        reactivity_result.onset_pulse_target, visual_config::kOnsetPulseDecayPerFrame);
    last_freq_band_ = reactivity.freq_band;

    const std::vector<Vector2> points = lissajous_math::to_points(
        features.waveform_l.data(), features.waveform_r.data(), features.waveform_l.size(),
        width_, height_, idle, reactivity_result.amplitude_scale,
        lissajous_math::kAmplitudeCompressionExponent);
    const float draw_thickness =
        waveform_math::pulse_thickness(params_.line_thickness, onset_pulse_);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Color segment_color = palette_override.has_value()
                                         ? palette_math::sample_by_index(*palette_override, i, points.size())
                                         : params_.line_color;
        DrawLineEx(points[i - 1], points[i], draw_thickness, segment_color);
    }
    EndTextureMode();
}
