#include "app/raylib_window_watcher.h"

#include <windows.h>

#include <commctrl.h>

namespace {

constexpr UINT_PTR kSubclassId = 1;

LRESULT CALLBACK WatcherSubclassProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR,
                                      DWORD_PTR ref_data) {
    if (msg == WM_EXITSIZEMOVE) {
        auto* callback = reinterpret_cast<std::function<void()>*>(ref_data);
        (*callback)();
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, WatcherSubclassProc, kSubclassId);
    }
    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

}

RaylibWindowWatcher::RaylibWindowWatcher(void* native_window_handle, std::function<void()> on_user_drag_or_resize)
    : hwnd_(native_window_handle), callback_(std::make_unique<std::function<void()>>(std::move(on_user_drag_or_resize))) {
    SetWindowSubclass(static_cast<HWND>(hwnd_), WatcherSubclassProc, kSubclassId,
                       reinterpret_cast<DWORD_PTR>(callback_.get()));
}

RaylibWindowWatcher::~RaylibWindowWatcher() {
    RemoveWindowSubclass(static_cast<HWND>(hwnd_), WatcherSubclassProc, kSubclassId);
}
