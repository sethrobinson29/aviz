#include "app/raylib_window_watcher.h"

#include <windows.h>

#include <commctrl.h>

struct RaylibWindowWatcher::State {
    std::function<void()> on_user_drag_or_resize;
    RECT rect_at_size_move_start{};
};

namespace {

constexpr UINT_PTR kSubclassId = 1;

LRESULT CALLBACK WatcherSubclassProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR,
                                      DWORD_PTR ref_data) {
    auto* state = reinterpret_cast<RaylibWindowWatcher::State*>(ref_data);
    if (msg == WM_ENTERSIZEMOVE) {
        GetWindowRect(hwnd, &state->rect_at_size_move_start);
    }
    if (msg == WM_EXITSIZEMOVE) {
        RECT now{};
        GetWindowRect(hwnd, &now);
        if (!EqualRect(&now, &state->rect_at_size_move_start)) {
            state->on_user_drag_or_resize();
        }
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, WatcherSubclassProc, kSubclassId);
    }
    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

}

RaylibWindowWatcher::RaylibWindowWatcher(void* native_window_handle, std::function<void()> on_user_drag_or_resize)
    : hwnd_(native_window_handle), state_(std::make_unique<State>()) {
    state_->on_user_drag_or_resize = std::move(on_user_drag_or_resize);
    SetWindowSubclass(static_cast<HWND>(hwnd_), WatcherSubclassProc, kSubclassId,
                       reinterpret_cast<DWORD_PTR>(state_.get()));
}

RaylibWindowWatcher::~RaylibWindowWatcher() {
    RemoveWindowSubclass(static_cast<HWND>(hwnd_), WatcherSubclassProc, kSubclassId);
}
