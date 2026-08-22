#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "raylib.h"
#include "visuals/visual_module.h"

namespace spectrum_math {

// Maps band energies to bar rectangles, one bar per band, left to right
// low-to-high frequency. Each band's height is band_energy[i] normalized
// against its own band_reference[i]; `band_reference[i] <= 0` yields a
// zero-height bar. `max_height_fraction` caps a fully-normalized bar short
// of the window edge. `idle` returns zero-height bars, ignoring both
// arrays.
std::vector<Rectangle> to_bars(const float* band_energy, const float* band_reference,
                                std::size_t band_count, int width, int height, float gap_ratio,
                                float max_height_fraction, bool idle);

}  // namespace spectrum_math

struct SpectrumParams {
    Color bar_color = WHITE;
    float gap_ratio = 0.2f;            // gap width as a fraction of bar width
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
};
