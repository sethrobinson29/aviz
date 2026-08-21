#pragma once

#include <cstddef>
#include <vector>

#include "raylib.h"
#include "visuals/visual_module.h"

namespace waveform_math {

// Maps a mono waveform sample window to screen-space points for a line
// strip. When `idle` is true, returns a flat mid-height line instead.
std::vector<Vector2> to_points(const float* samples, std::size_t count, int width, int height,
                                bool idle);

}  // namespace waveform_math

struct WaveformParams {
    Color line_color = ORANGE;
    float line_thickness = 3.0f;
};

class WaveformVisual : public VisualModule {
public:
    explicit WaveformVisual(WaveformParams params = {});
    ~WaveformVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features) override;
    RenderTexture2D& render_texture() override { return texture_; }

private:
    WaveformParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
};
