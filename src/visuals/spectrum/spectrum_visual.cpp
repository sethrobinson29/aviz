#include "visuals/spectrum/spectrum_visual.h"

#include <algorithm>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

namespace spectrum_math {

std::vector<Rectangle> to_bars(const float* band_energy, const float* band_reference,
                                std::size_t band_count, int width, int height, float gap_ratio,
                                float max_height_fraction, float sensitivity) {
    std::vector<Rectangle> bars;
    if (band_count == 0) {
        return bars;
    }
    bars.reserve(band_count);

    const float slot_width = static_cast<float>(width) / static_cast<float>(band_count);
    const float bar_width = slot_width / (1.0f + gap_ratio);
    const float gap = slot_width - bar_width;
    const float max_height = static_cast<float>(height) * max_height_fraction;

    for (std::size_t i = 0; i < band_count; ++i) {
        float normalized = 0.0f;
        if (band_reference[i] > 0.0f) {
            normalized = std::clamp(band_energy[i] / band_reference[i], 0.0f, 1.0f) * sensitivity;
        }
        const float bar_height = std::max(normalized * max_height, visual_config::kMinBarHeightPx);
        const float x = static_cast<float>(i) * slot_width + gap * 0.5f;
        const float y = static_cast<float>(height) - bar_height;
        bars.push_back(Rectangle{x, y, bar_width, bar_height});
    }
    return bars;
}

float update_onset_pulse(float current_pulse, bool onset, float sensitivity) {
    return onset ? sensitivity : current_pulse * visual_config::kSpectrumOnsetPulseDecayPerFrame;
}

Color pulse_color(Color base, float pulse) {
    pulse = std::clamp(pulse, 0.0f, 1.0f);
    auto rest_to_base = [pulse](unsigned char channel) {
        const float base_f = static_cast<float>(channel);
        const float rest = base_f * visual_config::kSpectrumOnsetPulseRestBrightnessFraction;
        const float lerped = rest + (base_f - rest) * pulse;
        return static_cast<unsigned char>(std::clamp(lerped, 0.0f, 255.0f));
    };
    return Color{rest_to_base(base.r), rest_to_base(base.g), rest_to_base(base.b), base.a};
}

std::vector<std::size_t> bar_indices_to_draw(std::optional<std::size_t> freq_band,
                                              std::size_t bar_count) {
    if (freq_band.has_value()) {
        if (*freq_band < bar_count) {
            return {*freq_band};
        }
        return {};
    }
    std::vector<std::size_t> indices(bar_count);
    for (std::size_t i = 0; i < bar_count; ++i) {
        indices[i] = i;
    }
    return indices;
}

}

SpectrumVisual::SpectrumVisual(SpectrumParams params) : params_(params) {}

SpectrumVisual::~SpectrumVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
}

void SpectrumVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void SpectrumVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity) {
    const bool onset_mode = reactivity.source == ReactivitySource::BandOnset ||
                             reactivity.source == ReactivitySource::GlobalBeat;

    for (std::size_t i = 0; i < audio_config::kTotalBandSlots; ++i) {
        onset_pulse_[i] = spectrum_math::update_onset_pulse(onset_pulse_[i], features.band_onset[i],
                                                              reactivity.sensitivity);
    }

    const std::vector<Rectangle> bars = spectrum_math::to_bars(
        features.band_energy.data(), features.band_agc_reference.data(), audio_config::kNumBands,
        width_, height_, params_.gap_ratio, params_.max_height_fraction, reactivity.sensitivity);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (std::size_t i : spectrum_math::bar_indices_to_draw(reactivity.freq_band, bars.size())) {
        Color draw_color = params_.bar_color;
        if (onset_mode) {
            draw_color = spectrum_math::pulse_color(params_.bar_color, onset_pulse_[i]);
        }
        DrawRectangleRec(bars[i], draw_color);
    }
    EndTextureMode();
}
