#include "app/window_layout_controller.h"

#include "app/window_layout_config.h"
#include "raylib.h"

WindowLayoutController::WindowLayoutController(Compositor& compositor, UIHost& ui_host)
    : compositor_(compositor), ui_host_(ui_host) {}

void WindowLayoutController::set_docked(bool docked) { apply(state_.set_docked(docked)); }

void WindowLayoutController::toggle_dock() { set_docked(mode() != WindowMode::Docked); }

void WindowLayoutController::set_fullscreen(bool fullscreen) { apply(state_.set_fullscreen(fullscreen)); }

void WindowLayoutController::toggle_fullscreen() { set_fullscreen(!is_fullscreen()); }

void WindowLayoutController::apply(const WindowLayoutAction& action) {
    if (action.enter_fullscreen) {
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
        reposition_visualizer();
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

void WindowLayoutController::reposition_visualizer() {
    const int monitor = GetCurrentMonitor();
    const DockedLayout layout =
        window_layout::compute_docked_layout(GetMonitorWidth(monitor), GetMonitorHeight(monitor));

    SetWindowPosition(layout.visualizer.x, layout.visualizer.y);
    SetWindowSize(layout.visualizer.width, layout.visualizer.height);
    compositor_.resize(layout.visualizer.width, layout.visualizer.height);
}

void WindowLayoutController::reposition_docked() {
    reposition_visualizer();

    const int monitor = GetCurrentMonitor();
    const DockedLayout layout =
        window_layout::compute_docked_layout(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    ui_host_.set_bounds(layout.panel.x, layout.panel.y, layout.panel.width, layout.panel.height);
}
