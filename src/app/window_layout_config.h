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
constexpr int kPanelWidth = 420;

DockedLayout compute_docked_layout(int monitor_width, int monitor_height);

}
