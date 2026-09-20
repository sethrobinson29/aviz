#pragma once

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
    void reposition_visualizer();
    void reposition_docked();

    WindowModeState state_;
    Compositor& compositor_;
    UIHost& ui_host_;
};
