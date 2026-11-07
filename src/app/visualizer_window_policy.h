#pragma once

enum class VisualizerHitRegion {
    Client,
    Caption,
};

enum class VisualizerCornerStyle {
    Rounded,
    Square,
};

namespace visualizer_window_policy {

VisualizerHitRegion hit_region(bool fullscreen);
VisualizerCornerStyle corner_style(bool fullscreen);

}
