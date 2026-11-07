#pragma once

#include "app/visualizer_chrome_theme.h"

struct OverlayRect {
    float x;
    float y;
    float width;
    float height;

    bool contains(float px, float py) const { return px >= x && px < x + width && py >= y && py < y + height; }
};

enum class OverlayButton {
    EnterFullscreen,
    ExitFullscreen,
};

struct OverlayLayout {
    OverlayButton button;
    OverlayRect rect;
    OverlayRect icon_in_button;
    float label_x;
    float label_y;
};

struct OverlayPointer {
    bool on_screen;
    float x;
    float y;
    bool pressed;
    bool released;
};

struct OverlayButtonState {
    bool hovered = false;
    bool armed = false;
};

struct OverlayStep {
    OverlayButtonState state;
    bool activated;
};

namespace visualizer_overlay {

OverlayLayout layout(const VisualizerChromeTheme& theme, bool fullscreen, float screen_width, float screen_height,
                     float dpi_scale, float label_width, float label_height);

OverlayStep step(OverlayButtonState state, const OverlayRect& rect, const OverlayPointer& pointer);

}
