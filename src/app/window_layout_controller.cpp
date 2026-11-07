#include "app/window_layout_controller.h"

#include <optional>

#include "raylib.h"

WindowLayoutController::WindowLayoutController(Compositor& compositor, UIHost& ui_host)
    : compositor_(compositor), ui_host_(ui_host) {}

void WindowLayoutController::set_docked(bool docked) { apply(state_.set_docked(docked)); }

void WindowLayoutController::toggle_dock() { set_docked(mode() != WindowMode::Docked); }

void WindowLayoutController::set_fullscreen(bool fullscreen) { apply(state_.set_fullscreen(fullscreen)); }

void WindowLayoutController::toggle_fullscreen() { set_fullscreen(!is_fullscreen()); }

void WindowLayoutController::apply(const WindowLayoutAction& action) {
    if (action.enter_fullscreen) {
        const Vector2 windowed_pos = GetWindowPosition();
        windowed_rect_ = WindowRect{static_cast<int>(windowed_pos.x), static_cast<int>(windowed_pos.y),
                                    GetScreenWidth(), GetScreenHeight()};
        const int monitor = GetCurrentMonitor();
        const Vector2 monitor_pos = GetMonitorPosition(monitor);
        const int monitor_width = GetMonitorWidth(monitor);
        const int monitor_height = GetMonitorHeight(monitor);

        SetWindowPosition(static_cast<int>(monitor_pos.x), static_cast<int>(monitor_pos.y));
        SetWindowSize(monitor_width, monitor_height);
        compositor_.resize(monitor_width, monitor_height);
        ToggleBorderlessWindowed();
    }
    if (action.exit_fullscreen) {
        ToggleBorderlessWindowed();
        SetWindowState(FLAG_WINDOW_UNDECORATED);
    }
    if (action.restore_windowed_rect) {
        place_visualizer(windowed_rect_);
    }
    if (action.hide_panel) {
        ui_host_.hide();
    }
    if (action.show_panel) {
        ui_host_.show();
    }
    if (action.reposition_docked) {
        reposition_docked();
    }
}

void WindowLayoutController::place_visualizer(const WindowRect& rect) {
    SetWindowPosition(rect.x, rect.y);
    SetWindowSize(rect.width, rect.height);
    compositor_.resize(rect.width, rect.height);
}

WindowRect WindowLayoutController::docking_monitor() const {
    const std::optional<ScreenRect> panel_monitor = ui_host_.panel_monitor_bounds();
    if (panel_monitor.has_value()) {
        return WindowRect{panel_monitor->x, panel_monitor->y, panel_monitor->width, panel_monitor->height};
    }
    const int monitor = GetCurrentMonitor();
    const Vector2 origin = GetMonitorPosition(monitor);
    return WindowRect{static_cast<int>(origin.x), static_cast<int>(origin.y), GetMonitorWidth(monitor),
                      GetMonitorHeight(monitor)};
}

void WindowLayoutController::reposition_docked() {
    const float panel_scale = ui_host_.panel_dpi_scale().value_or(GetWindowScaleDPI().x);
    const DockedLayout layout = window_layout::compute_docked_layout(docking_monitor(), panel_scale);
    place_visualizer(layout.visualizer);
    ui_host_.set_bounds(layout.panel.x, layout.panel.y, layout.panel.width, layout.panel.height);
}
