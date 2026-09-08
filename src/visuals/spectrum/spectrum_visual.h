#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "audio/audio_config.h"
#include "raylib.h"
#include "visuals/visual_module.h"

namespace spectrum_math {

std::vector<Rectangle> to_bars(const float* band_energy, const float* band_reference,
                                std::size_t band_count, int width, int height, float gap_ratio,
                                float max_height_fraction, float sensitivity = 1.0f);

std::vector<std::size_t> bar_indices_to_draw(std::optional<std::size_t> freq_band,
                                              std::size_t bar_count);

float update_onset_pulse(float current_pulse, bool onset, float sensitivity);

Color pulse_color(Color base, float pulse);

}

struct SpectrumParams {
    Color bar_color = WHITE;
    float gap_ratio = 0.2f;
    float max_height_fraction = 0.8f;
};

class SpectrumVisual : public VisualModule {
public:
    explicit SpectrumVisual(SpectrumParams params = {});
    ~SpectrumVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<SpectrumVisual>(params_);
    }
    const SpectrumParams& params() const { return params_; }

private:
    SpectrumParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    std::array<float, audio_config::kTotalBandSlots> onset_pulse_{};
};
