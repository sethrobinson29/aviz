#include "ui_host/ui_host.h"

#include <windows.h>

#include <wrl.h>
#include <wrl/client.h>

#include <WebView2.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ui_host/panel_dispatch_queue.h"
#include "ui_host/panel_message.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kWindowClassName[] = L"AvizControlPanel";
constexpr wchar_t kVirtualHostName[] = L"aviz.local";
constexpr wchar_t kStartPage[] = L"https://aviz.local/index.html";
constexpr wchar_t kTrustedPrefix[] = L"https://aviz.local/";
constexpr int kDefaultWidth = 420;
constexpr int kDefaultHeight = 720;

constexpr UINT kMsgShowPanel = WM_APP + 1;
constexpr UINT kMsgHidePanel = WM_APP + 2;

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
    return std::wcsncmp(uri, kTrustedPrefix, std::wcslen(kTrustedPrefix)) == 0;
}

}

class WebView2UIHost final : public UIHost {
public:
    explicit WebView2UIHost(std::string content_dir) : content_dir_(std::move(content_dir)) {
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

    std::vector<PanelMessage> drain_pending_messages() override { return queue_.drain(); }

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
                                     CW_USEDEFAULT, CW_USEDEFAULT, kDefaultWidth, kDefaultHeight, nullptr,
                                     nullptr, GetModuleHandleW(nullptr), this);
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
        webview3->SetVirtualHostNameToFolderMapping(kVirtualHostName, content_dir_w.c_str(),
                                                      COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);

        EventRegistrationToken token;
        webview_->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                    LPWSTR uri = nullptr;
                    if (SUCCEEDED(args->get_Uri(&uri)) && uri != nullptr) {
                        if (!is_trusted_uri(uri)) {
                            args->put_Cancel(TRUE);
                        }
                        CoTaskMemFree(uri);
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

                        std::optional<PanelMessage> message = panel_message::parse(utf8);
                        if (message.has_value()) {
                            queue_.push(*message);
                        }
                    }
                    return S_OK;
                })
                .Get(),
            &token);

        webview_->Navigate(kStartPage);
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
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
    }

    PanelDispatchQueue queue_;
    std::string content_dir_;
    std::thread thread_;

    std::mutex startup_mutex_;
    std::condition_variable startup_cv_;
    bool startup_failed_ = false;

    HWND hwnd_ = nullptr;
    std::atomic<bool> visible_{false};

    ComPtr<ICoreWebView2Environment> environment_;
    ComPtr<ICoreWebView2Controller> controller_;
    ComPtr<ICoreWebView2> webview_;
};

std::unique_ptr<UIHost> create_ui_host(const std::string& content_dir) {
    return std::make_unique<WebView2UIHost>(content_dir);
}
