#include "app/visualizer_overlay.h"

#include <algorithm>
#include <cmath>

namespace visualizer_overlay {

namespace {

float snap(float value) { return std::round(value); }

OverlayLayout fullscreen_toggle_layout(const FullscreenToggleTheme& toggle, float screen_width, float scale) {
    const float size = snap(toggle.size * scale);
    const float margin = snap(toggle.margin * scale);
    const float icon = snap(toggle.icon_size * scale);
    const OverlayRect rect{screen_width - margin - size, margin, size, size};
    const float icon_inset = snap((size - icon) * 0.5f);
    return OverlayLayout{OverlayButton::EnterFullscreen, rect, OverlayRect{icon_inset, icon_inset, icon, icon}, 0.0f,
                         0.0f};
}

OverlayLayout exit_pill_layout(const ExitFullscreenPillTheme& pill, float screen_width, float screen_height,
                               float scale, float label_width, float label_height) {
    const float icon = snap(pill.icon_size * scale);
    const float content_height = std::max(icon, label_height);
    const float height = snap(pill.padding_top * scale + content_height + pill.padding_bottom * scale);
    const float padding_left = snap(pill.padding_left * scale);
    const float gap = snap(pill.gap * scale);
    const float width = std::ceil(padding_left + icon + gap + label_width + pill.padding_right * scale);
    const float margin = snap(pill.margin * scale);
    const OverlayRect rect{screen_width - margin - width, screen_height - margin - height, width, height};
    const OverlayRect icon_rect{padding_left, snap((height - icon) * 0.5f), icon, icon};
    const float label_x = rect.x + padding_left + icon + gap;
    const float label_y = rect.y + snap((height - label_height) * 0.5f);
    return OverlayLayout{OverlayButton::ExitFullscreen, rect, icon_rect, label_x, label_y};
}

}

OverlayLayout layout(const VisualizerChromeTheme& theme, bool fullscreen, float screen_width, float screen_height,
                     float dpi_scale, float label_width, float label_height) {
    const float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    if (fullscreen) {
        return exit_pill_layout(theme.exit_pill, screen_width, screen_height, scale, std::max(label_width, 0.0f),
                                std::max(label_height, 0.0f));
    }
    return fullscreen_toggle_layout(theme.fullscreen_toggle, screen_width, scale);
}

OverlayStep step(OverlayButtonState state, const OverlayRect& rect, const OverlayPointer& pointer) {
    if (!pointer.on_screen) {
        return OverlayStep{OverlayButtonState{}, false};
    }
    OverlayButtonState next = state;
    next.hovered = rect.contains(pointer.x, pointer.y);
    if (pointer.pressed) {
        next.armed = next.hovered;
    }
    bool activated = false;
    if (pointer.released) {
        activated = next.armed && next.hovered;
        next.armed = false;
    }
    return OverlayStep{next, activated};
}

}
