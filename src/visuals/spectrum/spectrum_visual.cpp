#include "visuals/spectrum/spectrum_visual.h"

#include <algorithm>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

namespace spectrum_math {

std::vector<Rectangle> to_bars(const float* band_energy, const float* band_reference,
                                std::size_t band_count, int width, int height, float gap_ratio,
                                float max_height_fraction, bool idle) {
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
        if (!idle && band_reference[i] > 0.0f) {
            normalized = std::clamp(band_energy[i] / band_reference[i], 0.0f, 1.0f);
        }
        const float bar_height = normalized * max_height;
        const float x = static_cast<float>(i) * slot_width + gap * 0.5f;
        const float y = static_cast<float>(height) - bar_height;
        bars.push_back(Rectangle{x, y, bar_width, bar_height});
    }
    return bars;
}

}  // namespace spectrum_math

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

void SpectrumVisual::render(const AudioFeatures& features) {
    const bool idle = features.rms < visual_config::kIdleRmsFloor;
    const std::vector<Rectangle> bars = spectrum_math::to_bars(
        features.band_energy.data(), features.band_agc_reference.data(), audio_config::kNumBands,
        width_, height_, params_.gap_ratio, params_.max_height_fraction, idle);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (const Rectangle& bar : bars) {
        DrawRectangleRec(bar, params_.bar_color);
    }
    EndTextureMode();
}
