#include "visuals/waveform/waveform_visual.h"

#include <algorithm>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

namespace waveform_math {

std::vector<Vector2> to_points(const float* samples, std::size_t count, int width, int height,
                                bool idle, float amplitude_scale) {
    const float mid_y = static_cast<float>(height) * 0.5f;

    if (idle || count == 0) {
        return {Vector2{0.0f, mid_y}, Vector2{static_cast<float>(width), mid_y}};
    }

    std::vector<Vector2> points;
    points.reserve(count);
    const float x_step =
        count > 1 ? static_cast<float>(width) / static_cast<float>(count - 1) : 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float x = x_step * static_cast<float>(i);
        const float y = mid_y - samples[i] * amplitude_scale * mid_y;
        points.push_back(Vector2{x, y});
    }
    return points;
}

float band_amplitude_scale(float energy, float reference) {
    if (reference <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(energy / reference, 0.0f, 1.0f);
}

float pulse_thickness(float base_thickness, float pulse) {
    pulse = std::clamp(pulse, 0.0f, 1.0f);
    return base_thickness * (1.0f + (visual_config::kOnsetPulseThicknessBoost - 1.0f) * pulse);
}

ReactivityResult resolve_reactivity(const AudioFeatures& features, const LayerReactivity& reactivity) {
    ReactivityResult result;
    if (reactivity.freq_band.has_value()) {
        std::size_t band = *reactivity.freq_band;
        if (reactivity.source == ReactivitySource::BandAmplitude) {
            result.amplitude_scale =
                band_amplitude_scale(features.band_energy[band], features.band_agc_reference[band]) *
                reactivity.sensitivity;
        } else if (reactivity.source == ReactivitySource::BandOnset) {
            result.onset_active = features.band_onset[band];
        }
    } else if (reactivity.source == ReactivitySource::GlobalBeat) {
        result.onset_active = features.band_onset[audio_config::kGlobalBandIndex];
    }
    if (result.onset_active) {
        result.onset_pulse_target = reactivity.sensitivity;
    }
    return result;
}

}  // namespace waveform_math

WaveformVisual::WaveformVisual(WaveformParams params) : params_(params) {}

WaveformVisual::~WaveformVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
}

void WaveformVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void WaveformVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity) {
    const std::size_t idle_band = reactivity.freq_band.value_or(audio_config::kGlobalBandIndex);
    const bool idle = features.band_idle[idle_band];

    const waveform_math::ReactivityResult reactivity_result =
        waveform_math::resolve_reactivity(features, reactivity);
    onset_pulse_ = reactivity_result.onset_active
                       ? reactivity_result.onset_pulse_target
                       : onset_pulse_ * visual_config::kOnsetPulseDecayPerFrame;

    const std::vector<Vector2> points =
        waveform_math::to_points(features.waveform.data(), features.waveform.size(), width_,
                                  height_, idle, reactivity_result.amplitude_scale);
    const float draw_thickness =
        waveform_math::pulse_thickness(params_.line_thickness, onset_pulse_);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (std::size_t i = 1; i < points.size(); ++i) {
        DrawLineEx(points[i - 1], points[i], draw_thickness, params_.line_color);
    }
    EndTextureMode();
}
