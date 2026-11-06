#include "ui_host/ui_host.h"

#include <windows.h>

#include <commdlg.h>
#include <wrl.h>
#include <wrl/client.h>

#include <WebView2.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "ui_host/panel_dispatch_queue.h"
#include "ui_host/panel_message.h"
#include "ui_host/panel_navigation_policy.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kWindowClassName[] = L"AvizControlPanel";
constexpr int kBlockedResourceStatusCode = 403;

#ifdef NDEBUG
constexpr wchar_t kAdditionalBrowserArgumentsVariable[] = L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS";
#endif

constexpr UINT kMsgShowPanel = WM_APP + 1;
constexpr UINT kMsgHidePanel = WM_APP + 2;
constexpr UINT kMsgSendToPanel = WM_APP + 3;
constexpr UINT kMsgSetBounds = WM_APP + 4;
constexpr UINT kMsgShowAlert = WM_APP + 5;
constexpr wchar_t kAlertTitle[] = L"aviz";
constexpr DWORD kPresetDialogPathCapacity = 32768;
constexpr char kOpenAudioFileDialogRequest[] = "open_file_dialog";
constexpr char kOpenPresetFileDialogRequest[] = "open_preset_dialog";

struct BoundsPayload {
    int x;
    int y;
    int width;
    int height;
};

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::string wide_to_utf8(const wchar_t* text) {
    if (text == nullptr) {
        return std::string();
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return std::string();
    }
    std::string utf8(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), size, nullptr, nullptr);
    return utf8;
}

bool is_trusted_uri(const wchar_t* uri) {
    if (uri == nullptr) {
        return false;
    }
    return panel_navigation_policy::is_trusted_uri(uri);
}

template <typename NavigationArgs>
void cancel_navigation_unless_trusted(NavigationArgs* args) {
    LPWSTR uri = nullptr;
    const bool trusted = SUCCEEDED(args->get_Uri(&uri)) && is_trusted_uri(uri);
    if (uri != nullptr) {
        CoTaskMemFree(uri);
    }
    if (!trusted) {
        args->put_Cancel(TRUE);
    }
}

std::string native_dialog_request_type(const std::string& json_text) {
    try {
        nlohmann::json doc = nlohmann::json::parse(json_text);
        if (!doc.is_object()) {
            return std::string();
        }
        const std::string type = doc.value("type", std::string());
        if (type == kOpenAudioFileDialogRequest || type == kOpenPresetFileDialogRequest) {
            return type;
        }
        return std::string();
    } catch (const nlohmann::json::exception&) {
        return std::string();
    }
}

std::optional<std::string> show_open_preset_file_dialog(HWND owner, const std::string& presets_dir) {
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::u8path(presets_dir), ignored);
    const std::wstring initial_dir = utf8_to_wide(presets_dir);

    std::vector<wchar_t> file_buffer(kPresetDialogPathCapacity, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Layer presets (*.json)\0*.json\0";
    ofn.lpstrFile = file_buffer.data();
    ofn.nMaxFile = kPresetDialogPathCapacity;
    ofn.lpstrInitialDir = initial_dir.empty() ? nullptr : initial_dir.c_str();
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }
    return wide_to_utf8(file_buffer.data());
}

std::optional<std::string> show_open_audio_file_dialog(HWND owner) {
    wchar_t file_buffer[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Audio Files\0*.wav;*.mp3;*.flac\0All Files\0*.*\0";
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }
    return wide_to_utf8(file_buffer);
}

}

class WebView2UIHost final : public UIHost {
public:
    WebView2UIHost(std::string content_dir, std::string presets_dir, int initial_x, int initial_y,
                   int initial_width, int initial_height)
        : content_dir_(std::move(content_dir)),
          presets_dir_(std::move(presets_dir)),
          initial_bounds_{initial_x, initial_y, initial_width, initial_height} {
        std::unique_lock<std::mutex> lock(startup_mutex_);
        thread_ = std::thread(&WebView2UIHost::thread_main, this);
        startup_cv_.wait(lock, [this] { return hwnd_ != nullptr || startup_failed_; });
    }

    ~WebView2UIHost() override {
        if (hwnd_ != nullptr) {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void show() override {
        if (hwnd_ != nullptr) {
            PostMessageW(hwnd_, kMsgShowPanel, 0, 0);
        }
    }

    void hide() override {
        if (hwnd_ != nullptr) {
            PostMessageW(hwnd_, kMsgHidePanel, 0, 0);
        }
    }

    bool is_visible() const override { return visible_.load(); }

    bool should_close() const override { return should_close_.load(); }

    void set_bounds(int x, int y, int width, int height) override {
        if (hwnd_ == nullptr) {
            return;
        }
        auto* payload = new BoundsPayload{x, y, width, height};
        if (!PostMessageW(hwnd_, kMsgSetBounds, 0, reinterpret_cast<LPARAM>(payload))) {
            delete payload;
        }
    }

    std::vector<PanelMessage> drain_pending_messages() override { return queue_.drain(); }

    void send_to_panel(const std::string& json_text) override {
        if (hwnd_ == nullptr) {
            return;
        }
        auto* payload = new std::wstring(utf8_to_wide(json_text));
        if (!PostMessageW(hwnd_, kMsgSendToPanel, 0, reinterpret_cast<LPARAM>(payload))) {
            delete payload;
        }
    }

    void show_alert(const std::string& message) override {
        if (hwnd_ == nullptr) {
            return;
        }
        auto* payload = new std::wstring(utf8_to_wide(message));
        if (!PostMessageW(hwnd_, kMsgShowAlert, 0, reinterpret_cast<LPARAM>(payload))) {
            delete payload;
        }
    }

    std::optional<ScreenRect> panel_monitor_bounds() const override {
        if (hwnd_ == nullptr) {
            return std::nullopt;
        }
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &info)) {
            return std::nullopt;
        }
        const RECT& r = info.rcMonitor;
        return ScreenRect{r.left, r.top, r.right - r.left, r.bottom - r.top};
    }

private:
    void thread_main() {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
            fail_startup();
            return;
        }

        WNDCLASSW wc{};
        wc.lpfnWndProc = &WebView2UIHost::WndProcThunk;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kWindowClassName;
        wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        RegisterClassW(&wc);

        HWND hwnd = CreateWindowExW(0, kWindowClassName, L"aviz control panel", WS_OVERLAPPEDWINDOW,
                                     initial_bounds_.x, initial_bounds_.y, initial_bounds_.width,
                                     initial_bounds_.height, nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (hwnd == nullptr) {
            CoUninitialize();
            fail_startup();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(startup_mutex_);
            hwnd_ = hwnd;
        }
        startup_cv_.notify_all();

        ShowWindow(hwnd, SW_SHOW);
        visible_.store(true);

        start_webview2(hwnd);

        MSG msg;
        BOOL got_message;
        while ((got_message = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        controller_.Reset();
        webview_.Reset();
        environment_.Reset();
        CoUninitialize();
    }

    void fail_startup() {
        std::lock_guard<std::mutex> lock(startup_mutex_);
        startup_failed_ = true;
        startup_cv_.notify_all();
    }

    void start_webview2(HWND hwnd) {
        std::wstring content_dir_w = utf8_to_wide(content_dir_);
        auto env_completed = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd, content_dir_w](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || env == nullptr) {
                    std::fprintf(stderr, "ui_host: failed creating WebView2 environment (hr=0x%08lx)\n",
                                 static_cast<unsigned long>(result));
                    return S_OK;
                }
                environment_ = env;
                auto ctrl_completed = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this, hwnd, content_dir_w](HRESULT ctrl_result,
                                                 ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(ctrl_result) || controller == nullptr) {
                            std::fprintf(stderr, "ui_host: failed creating WebView2 controller (hr=0x%08lx)\n",
                                         static_cast<unsigned long>(ctrl_result));
                            return S_OK;
                        }
                        on_controller_ready(hwnd, controller, content_dir_w);
                        return S_OK;
                    });
                environment_->CreateCoreWebView2Controller(hwnd, ctrl_completed.Get());
                return S_OK;
            });
#ifdef NDEBUG
        SetEnvironmentVariableW(kAdditionalBrowserArgumentsVariable, nullptr);
#endif
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, env_completed.Get());
        if (FAILED(hr)) {
            std::fprintf(stderr, "ui_host: CreateCoreWebView2EnvironmentWithOptions failed (hr=0x%08lx)\n",
                         static_cast<unsigned long>(hr));
        }
    }

    void on_controller_ready(HWND hwnd, ICoreWebView2Controller* controller, const std::wstring& content_dir_w) {
        controller_ = controller;
        controller_->get_CoreWebView2(&webview_);
        if (!webview_) {
            std::fprintf(stderr, "ui_host: controller has no CoreWebView2 -- panel content not loaded\n");
            return;
        }

        RECT bounds;
        GetClientRect(hwnd, &bounds);
        controller_->put_Bounds(bounds);

        ComPtr<ICoreWebView2Settings> settings;
        webview_->get_Settings(&settings);
        if (settings) {
#ifdef NDEBUG
            settings->put_AreDevToolsEnabled(FALSE);
#endif
        }

        ComPtr<ICoreWebView2_3> webview3;
        if (FAILED(webview_.As(&webview3)) || !webview3) {
            std::fprintf(stderr,
                         "ui_host: ICoreWebView2_3 unavailable -- cannot scope local file access, panel "
                         "content not loaded\n");
            return;
        }
        webview3->SetVirtualHostNameToFolderMapping(panel_navigation_policy::kVirtualHostName,
                                                      content_dir_w.c_str(),
                                                      COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);

        EventRegistrationToken token;
        webview_->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                    cancel_navigation_unless_trusted(args);
                    return S_OK;
                })
                .Get(),
            &token);

        webview_->add_FrameNavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                    cancel_navigation_unless_trusted(args);
                    return S_OK;
                })
                .Get(),
            &token);

        webview_->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                    args->put_Handled(TRUE);
                    return S_OK;
                })
                .Get(),
            &token);

        webview_->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        webview_->add_WebResourceRequested(
            Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                    ComPtr<ICoreWebView2WebResourceRequest> request;
                    LPWSTR uri = nullptr;
                    const bool trusted = SUCCEEDED(args->get_Request(&request)) && request &&
                                         SUCCEEDED(request->get_Uri(&uri)) && is_trusted_uri(uri);
                    if (uri != nullptr) {
                        CoTaskMemFree(uri);
                    }
                    if (!trusted) {
                        ComPtr<ICoreWebView2WebResourceResponse> blocked;
                        if (SUCCEEDED(environment_->CreateWebResourceResponse(
                                nullptr, kBlockedResourceStatusCode, L"Blocked", L"", &blocked))) {
                            args->put_Response(blocked.Get());
                        }
                    }
                    return S_OK;
                })
                .Get(),
            &token);

        webview_->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    LPWSTR raw = nullptr;
                    if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw != nullptr) {
                        std::string utf8 = wide_to_utf8(raw);
                        CoTaskMemFree(raw);

                        const std::string dialog_request = native_dialog_request_type(utf8);
                        if (dialog_request == kOpenAudioFileDialogRequest) {
                            std::optional<std::string> path = show_open_audio_file_dialog(hwnd_);
                            if (path.has_value()) {
                                queue_.push(PanelMessage{ImportFileMessage{*path}});
                            }
                        } else if (dialog_request == kOpenPresetFileDialogRequest) {
                            std::optional<std::string> path = show_open_preset_file_dialog(hwnd_, presets_dir_);
                            if (path.has_value()) {
                                queue_.push(PanelMessage{LoadPresetFileMessage{*path}});
                            }
                        } else {
                            std::optional<PanelMessage> message = panel_message::parse(utf8);
                            if (message.has_value()) {
                                queue_.push(*message);
                            }
                        }
                    }
                    return S_OK;
                })
                .Get(),
            &token);

        webview_->Navigate(panel_navigation_policy::kStartPage);
    }

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        WebView2UIHost* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<WebView2UIHost*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<WebView2UIHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self != nullptr) {
            return self->wnd_proc(hwnd, msg, wparam, lparam);
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    LRESULT wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        switch (msg) {
            case kMsgShowPanel:
                ShowWindow(hwnd, SW_SHOW);
                if (controller_) {
                    controller_->put_IsVisible(TRUE);
                }
                visible_.store(true);
                return 0;
            case kMsgHidePanel:
                ShowWindow(hwnd, SW_HIDE);
                if (controller_) {
                    controller_->put_IsVisible(FALSE);
                }
                visible_.store(false);
                return 0;
            case kMsgSetBounds: {
                std::unique_ptr<BoundsPayload> payload(reinterpret_cast<BoundsPayload*>(lparam));
                SetWindowPos(hwnd, nullptr, payload->x, payload->y, payload->width, payload->height,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            case WM_ENTERSIZEMOVE:
                GetWindowRect(hwnd, &rect_at_size_move_start_);
                return DefWindowProcW(hwnd, msg, wparam, lparam);
            case WM_EXITSIZEMOVE: {
                RECT now{};
                GetWindowRect(hwnd, &now);
                if (!EqualRect(&now, &rect_at_size_move_start_)) {
                    queue_.push(PanelMessage{SetDockedMessage{false}});
                }
                return DefWindowProcW(hwnd, msg, wparam, lparam);
            }
            case kMsgShowAlert: {
                std::unique_ptr<std::wstring> payload(reinterpret_cast<std::wstring*>(lparam));
                MessageBoxW(hwnd, payload->c_str(), kAlertTitle, MB_OK | MB_ICONWARNING);
                return 0;
            }
            case kMsgSendToPanel: {
                std::unique_ptr<std::wstring> payload(reinterpret_cast<std::wstring*>(lparam));
                if (webview_) {
                    webview_->PostWebMessageAsString(payload->c_str());
                }
                return 0;
            }
            case WM_SIZE: {
                if (controller_) {
                    RECT bounds;
                    GetClientRect(hwnd, &bounds);
                    controller_->put_Bounds(bounds);
                }
                return 0;
            }
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                should_close_.store(true);
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
    }

    PanelDispatchQueue queue_;
    std::string content_dir_;
    std::string presets_dir_;
    RECT rect_at_size_move_start_{};
    BoundsPayload initial_bounds_;
    std::thread thread_;

    std::mutex startup_mutex_;
    std::condition_variable startup_cv_;
    bool startup_failed_ = false;

    HWND hwnd_ = nullptr;
    std::atomic<bool> visible_{false};
    std::atomic<bool> should_close_{false};

    ComPtr<ICoreWebView2Environment> environment_;
    ComPtr<ICoreWebView2Controller> controller_;
    ComPtr<ICoreWebView2> webview_;
};

std::unique_ptr<UIHost> create_ui_host(const std::string& content_dir, const std::string& presets_dir,
                                        int initial_x, int initial_y, int initial_width, int initial_height) {
    return std::make_unique<WebView2UIHost>(content_dir, presets_dir, initial_x, initial_y, initial_width,
                                            initial_height);
}
