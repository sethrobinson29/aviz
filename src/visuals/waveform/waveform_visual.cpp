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

float next_onset_pulse(float current_pulse, bool band_changed, bool onset_active,
                        float pulse_target, float decay_per_frame) {
    const float base_pulse = band_changed ? 0.0f : current_pulse;
    return onset_active ? pulse_target : base_pulse * decay_per_frame;
}

ReactivityResult resolve_reactivity(const AudioFeatures& features, const LayerReactivity& reactivity) {
    ReactivityResult result;
    if (reactivity.freq_band.has_value()) {
        const std::size_t band = *reactivity.freq_band;
        if (band < audio_config::kNumBands) {
            if (reactivity.source == ReactivitySource::BandAmplitude) {
                result.amplitude_scale =
                    band_amplitude_scale(features.band_energy[band], features.band_agc_reference[band]) *
                    reactivity.sensitivity;
            } else if (reactivity.source == ReactivitySource::BandOnset) {
                result.onset_active = features.band_onset[band];
            }
        }
    } else if (reactivity.source == ReactivitySource::GlobalBeat) {
        result.onset_active = features.band_onset[audio_config::kGlobalBandIndex];
    }
    if (result.onset_active) {
        result.onset_pulse_target = reactivity.sensitivity;
    }
    return result;
}

std::size_t safe_band_index(std::optional<std::size_t> freq_band) {
    if (freq_band.has_value() && *freq_band < audio_config::kNumBands) {
        return *freq_band;
    }
    return audio_config::kGlobalBandIndex;
}

}

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

void WaveformVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity,
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

    const std::vector<Vector2> points =
        waveform_math::to_points(features.waveform.data(), features.waveform.size(), width_,
                                  height_, idle, reactivity_result.amplitude_scale);
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
