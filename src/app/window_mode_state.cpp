#include "app/window_mode_state.h"

namespace {

bool panel_should_be_visible(WindowMode mode, bool fullscreen) {
    return !(fullscreen && mode == WindowMode::Docked);
}

void apply_visibility_transition(WindowLayoutAction& action, bool was_visible, bool now_visible) {
    if (now_visible && !was_visible) {
        action.show_panel = true;
    } else if (!now_visible && was_visible) {
        action.hide_panel = true;
    }
}

}

WindowLayoutAction WindowModeState::set_docked(bool docked) {
    WindowLayoutAction action;
    const bool currently_docked = mode_ == WindowMode::Docked;
    if (docked == currently_docked) {
        return action;
    }

    const bool was_visible = panel_should_be_visible(mode_, fullscreen_);
    mode_ = docked ? WindowMode::Docked : WindowMode::Detached;
    apply_visibility_transition(action, was_visible, panel_should_be_visible(mode_, fullscreen_));

    if (docked && !fullscreen_) {
        action.reposition_docked = true;
    }
    return action;
}

WindowLayoutAction WindowModeState::set_fullscreen(bool fullscreen) {
    WindowLayoutAction action;
    if (fullscreen == fullscreen_) {
        return action;
    }

    const bool was_visible = panel_should_be_visible(mode_, fullscreen_);
    fullscreen_ = fullscreen;

    if (fullscreen_) {
        action.enter_fullscreen = true;
    } else {
        action.exit_fullscreen = true;
        if (mode_ == WindowMode::Docked) {
            action.reposition_docked = true;
        }
    }

    apply_visibility_transition(action, was_visible, panel_should_be_visible(mode_, fullscreen_));
    return action;
}
