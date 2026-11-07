#pragma once

#include <functional>
#include <memory>

class RaylibWindowWatcher {
public:
    RaylibWindowWatcher(void* native_window_handle, std::function<void()> on_user_drag_or_resize,
                        std::function<bool()> is_fullscreen,
                        std::function<bool(int client_x, int client_y)> is_over_overlay_button);
    ~RaylibWindowWatcher();

    RaylibWindowWatcher(const RaylibWindowWatcher&) = delete;
    RaylibWindowWatcher& operator=(const RaylibWindowWatcher&) = delete;

    struct State;

private:
    void* hwnd_;
    std::unique_ptr<State> state_;
};
