#include "app/window_layout_config.h"

#include <algorithm>

namespace window_layout {

DockedLayout compute_docked_layout(int monitor_width, int monitor_height) {
    const int combined_width = kVisualizerWidth + kPanelWidth;
    const int anchor_x = std::max(0, (monitor_width - combined_width) / 2);
    const int anchor_y = std::max(0, (monitor_height - kVisualizerHeight) / 2);

    DockedLayout layout;
    layout.visualizer = WindowRect{anchor_x, anchor_y, kVisualizerWidth, kVisualizerHeight};
    layout.panel = WindowRect{anchor_x + kVisualizerWidth, anchor_y, kPanelWidth, kVisualizerHeight};
    return layout;
}

}
