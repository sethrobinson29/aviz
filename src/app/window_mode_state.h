#pragma once

enum class WindowMode { Docked, Detached };

struct WindowLayoutAction {
    bool reposition_docked = false;
    bool show_panel = false;
    bool hide_panel = false;
    bool enter_fullscreen = false;
    bool exit_fullscreen = false;
    bool restore_windowed_rect = false;
};

class WindowModeState {
public:
    WindowMode mode() const { return mode_; }
    bool is_fullscreen() const { return fullscreen_; }

    WindowLayoutAction set_docked(bool docked);
    WindowLayoutAction set_fullscreen(bool fullscreen);

private:
    WindowMode mode_ = WindowMode::Docked;
    bool fullscreen_ = false;
};
