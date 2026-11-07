#include "app/visualizer_window_policy.h"

namespace visualizer_window_policy {

VisualizerHitRegion hit_region(bool fullscreen) {
    return fullscreen ? VisualizerHitRegion::Client : VisualizerHitRegion::Caption;
}

VisualizerCornerStyle corner_style(bool fullscreen) {
    return fullscreen ? VisualizerCornerStyle::Square : VisualizerCornerStyle::Rounded;
}

}
