#pragma once

struct WindowRect {
    int x;
    int y;
    int width;
    int height;
};

struct DockedLayout {
    WindowRect visualizer;
    WindowRect panel;
};

namespace window_layout {

constexpr int kVisualizerWidth = 1280;
constexpr int kVisualizerHeight = 720;
constexpr int kPanelWidthDip = 420;

int panel_width_for_scale(float panel_dpi_scale);

DockedLayout compute_docked_layout(const WindowRect& monitor, float panel_dpi_scale);

}
