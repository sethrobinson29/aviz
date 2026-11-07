#include "app/window_layout_config.h"

#include <algorithm>
#include <cmath>

namespace window_layout {

int panel_width_for_scale(float panel_dpi_scale) {
    const float scale = std::isfinite(panel_dpi_scale) && panel_dpi_scale > 0.0f ? panel_dpi_scale : 1.0f;
    return static_cast<int>(std::lround(static_cast<float>(kPanelWidthDip) * scale));
}

DockedLayout compute_docked_layout(const WindowRect& monitor, float panel_dpi_scale) {
    const int panel_width = panel_width_for_scale(panel_dpi_scale);
    const int combined_width = kVisualizerWidth + panel_width;
    const int anchor_x = monitor.x + std::max(0, (monitor.width - combined_width) / 2);
    const int anchor_y = monitor.y + std::max(0, (monitor.height - kVisualizerHeight) / 2);

    DockedLayout layout;
    layout.visualizer = WindowRect{anchor_x, anchor_y, kVisualizerWidth, kVisualizerHeight};
    layout.panel = WindowRect{anchor_x + kVisualizerWidth, anchor_y, panel_width, kVisualizerHeight};
    return layout;
}

}
