#pragma once

#include "app/window_layout_config.h"
#include "app/window_mode_state.h"
#include "compositor/compositor.h"
#include "ui_host/ui_host.h"

class WindowLayoutController {
public:
    WindowLayoutController(Compositor& compositor, UIHost& ui_host);

    void set_docked(bool docked);
    void toggle_dock();

    void set_fullscreen(bool fullscreen);
    void toggle_fullscreen();

    WindowMode mode() const { return state_.mode(); }
    bool is_fullscreen() const { return state_.is_fullscreen(); }

private:
    void apply(const WindowLayoutAction& action);
    void place_visualizer(const WindowRect& rect);
    void reposition_docked();
    WindowRect docking_monitor() const;

    WindowModeState state_;
    WindowRect windowed_rect_{0, 0, window_layout::kVisualizerWidth, window_layout::kVisualizerHeight};
    Compositor& compositor_;
    UIHost& ui_host_;
};
