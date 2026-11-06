#pragma once

#include <functional>
#include <memory>

class RaylibWindowWatcher {
public:
    RaylibWindowWatcher(void* native_window_handle, std::function<void()> on_user_drag_or_resize);
    ~RaylibWindowWatcher();

    RaylibWindowWatcher(const RaylibWindowWatcher&) = delete;
    RaylibWindowWatcher& operator=(const RaylibWindowWatcher&) = delete;

    struct State;

private:
    void* hwnd_;
    std::unique_ptr<State> state_;
};
