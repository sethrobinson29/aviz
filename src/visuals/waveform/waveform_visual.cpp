#include "visuals/waveform/waveform_visual.h"

#include "visuals/visual_config.h"

namespace waveform_math {

std::vector<Vector2> to_points(const float* samples, std::size_t count, int width, int height,
                                bool idle) {
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
        const float y = mid_y - samples[i] * mid_y;
        points.push_back(Vector2{x, y});
    }
    return points;
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

void WaveformVisual::render(const AudioFeatures& features) {
    const bool idle = features.rms < visual_config::kIdleRmsFloor;
    const std::vector<Vector2> points = waveform_math::to_points(
        features.waveform.data(), features.waveform.size(), width_, height_, idle);

    BeginTextureMode(texture_);
    ClearBackground(BLACK);
    DrawLineStrip(points.data(), static_cast<int>(points.size()), params_.line_color);
    EndTextureMode();
}
