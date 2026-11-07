#include "app/raylib_window_watcher.h"

#include <windows.h>

#include <commctrl.h>
#include <dwmapi.h>

#include <cstdio>
#include <optional>

#include "app/visualizer_window_policy.h"

struct RaylibWindowWatcher::State {
    std::function<void()> on_user_drag_or_resize;
    std::function<bool()> is_fullscreen;
    RECT rect_at_size_move_start{};
    std::optional<VisualizerCornerStyle> applied_corner_style;
};

namespace {

constexpr UINT_PTR kSubclassId = 1;

void apply_corner_style(HWND hwnd, RaylibWindowWatcher::State& state) {
    const VisualizerCornerStyle wanted = visualizer_window_policy::corner_style(state.is_fullscreen());
    if (state.applied_corner_style == wanted) {
        return;
    }
    const DWM_WINDOW_CORNER_PREFERENCE preference =
        wanted == VisualizerCornerStyle::Rounded ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
    const HRESULT hr =
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
    if (FAILED(hr)) {
        std::fprintf(stderr, "visualizer: window corner preference unavailable (hr=0x%08lx)\n",
                     static_cast<unsigned long>(hr));
    }
    state.applied_corner_style = wanted;
}

LRESULT CALLBACK WatcherSubclassProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR,
                                      DWORD_PTR ref_data) {
    auto* state = reinterpret_cast<RaylibWindowWatcher::State*>(ref_data);
    switch (msg) {
        case WM_NCHITTEST: {
            const LRESULT hit = DefSubclassProc(hwnd, msg, wparam, lparam);
            if (hit != HTCLIENT) {
                return hit;
            }
            return visualizer_window_policy::hit_region(state->is_fullscreen()) == VisualizerHitRegion::Caption
                       ? HTCAPTION
                       : HTCLIENT;
        }
        case WM_NCLBUTTONDBLCLK:
            if (wparam == HTCAPTION) {
                return 0;
            }
            break;
        case WM_WINDOWPOSCHANGED:
            apply_corner_style(hwnd, *state);
            break;
        case WM_ENTERSIZEMOVE:
            GetWindowRect(hwnd, &state->rect_at_size_move_start);
            break;
        case WM_EXITSIZEMOVE: {
            RECT now{};
            GetWindowRect(hwnd, &now);
            if (!EqualRect(&now, &state->rect_at_size_move_start)) {
                state->on_user_drag_or_resize();
            }
            break;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, WatcherSubclassProc, kSubclassId);
            break;
        default:
            break;
    }
    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

}

RaylibWindowWatcher::RaylibWindowWatcher(void* native_window_handle, std::function<void()> on_user_drag_or_resize,
                                         std::function<bool()> is_fullscreen)
    : hwnd_(native_window_handle), state_(std::make_unique<State>()) {
    state_->on_user_drag_or_resize = std::move(on_user_drag_or_resize);
    state_->is_fullscreen = std::move(is_fullscreen);
    HWND hwnd = static_cast<HWND>(hwnd_);
    apply_corner_style(hwnd, *state_);
    SetWindowSubclass(hwnd, WatcherSubclassProc, kSubclassId, reinterpret_cast<DWORD_PTR>(state_.get()));
}

RaylibWindowWatcher::~RaylibWindowWatcher() {
    RemoveWindowSubclass(static_cast<HWND>(hwnd_), WatcherSubclassProc, kSubclassId);
}
