#include "gui/overlay.h"
#include <wrl.h>
#include <WebView2.h>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace Microsoft::WRL;

namespace gui {

Overlay& Overlay::Get() {
    static Overlay instance;
    return instance;
}

static const wchar_t* WINDOW_CLASS = L"KoxpOverlay";
static const wchar_t* WINDOW_TITLE = L"KOXP Bot";

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], len);
    return wstr;
}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], len, nullptr, nullptr);
    return str;
}

static std::string LoadHtmlFile() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string dir(exePath);
    dir = dir.substr(0, dir.find_last_of('\\') + 1);
    std::string htmlPath = dir + "ui\\index.html";

    std::ifstream file(htmlPath);
    if (!file.is_open()) {
        printf("[KOXP] UI dosyasi bulunamadi: %s\n", htmlPath.c_str());
        return "<html><body style='background:#0F0F23;color:white'><h1>UI yuklenemedi</h1></body></html>";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: {
            auto* self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (self && self->webviewController_) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                auto* controller = static_cast<ICoreWebView2Controller*>(self->webviewController_);
                controller->put_Bounds(bounds);
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Overlay::CreateOverlayWindow(HMODULE hModule) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hModule;
    wc.lpszClassName  = WINDOW_CLASS;
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(15, 15, 35));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST,
        WINDOW_CLASS, WINDOW_TITLE,
        WS_POPUP | WS_VISIBLE | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        100, 100, 460, 720,
        nullptr, nullptr, hModule, nullptr);

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

void Overlay::InitWebView() {
    auto htmlContent = LoadHtmlFile();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, htmlContent](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    printf("[KOXP] WebView2 environment olusturulamadi: 0x%08X\n", result);
                    return result;
                }

                env->CreateCoreWebView2Controller(hwnd_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, htmlContent](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) {
                                printf("[KOXP] WebView2 controller olusturulamadi: 0x%08X\n", result);
                                return result;
                            }

                            webviewController_ = controller;
                            controller->AddRef();

                            ICoreWebView2* view = nullptr;
                            controller->get_CoreWebView2(&view);
                            webviewView_ = view;

                            RECT bounds;
                            GetClientRect(hwnd_, &bounds);
                            controller->put_Bounds(bounds);

                            ICoreWebView2Settings* settings = nullptr;
                            view->get_Settings(&settings);
                            if (settings) {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_AreDefaultScriptDialogsEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->put_AreDevToolsEnabled(TRUE);
                                settings->Release();
                            }

                            auto wHtml = Utf8ToWide(htmlContent);
                            view->NavigateToString(wHtml.c_str());

                            view->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR msg = nullptr;
                                        args->TryGetWebMessageAsString(&msg);
                                        if (msg) {
                                            std::wstring wmsg(msg);
                                            std::string utf8 = WideToUtf8(wmsg);
                                            if (messageHandler_)
                                                messageHandler_(utf8);
                                            CoTaskMemFree(msg);
                                        }
                                        return S_OK;
                                    }).Get(), nullptr);

                            ready_ = true;
                            printf("[KOXP] WebView2 hazir\n");

                            ProcessMessageQueue();

                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        printf("[KOXP] CreateCoreWebView2Environment basarisiz: 0x%08X\n", hr);
    }
}

bool Overlay::Initialize(HMODULE hModule) {
    CreateOverlayWindow(hModule);
    InitWebView();
    return hwnd_ != nullptr;
}

void Overlay::Shutdown() {
    ready_ = false;

    if (webviewController_) {
        static_cast<ICoreWebView2Controller*>(webviewController_)->Close();
        static_cast<ICoreWebView2Controller*>(webviewController_)->Release();
        webviewController_ = nullptr;
    }
    if (webviewView_) {
        static_cast<ICoreWebView2*>(webviewView_)->Release();
        webviewView_ = nullptr;
    }

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(WINDOW_CLASS, nullptr);
}

void Overlay::PostMessage(const std::string& json) {
    if (!ready_ || !webviewView_) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        outQueue_.push(json);
        return;
    }

    auto* view = static_cast<ICoreWebView2*>(webviewView_);
    auto wjson = Utf8ToWide(json);
    view->PostWebMessageAsJson(wjson.c_str());
}

void Overlay::ProcessMessageQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!outQueue_.empty()) {
        auto& msg = outQueue_.front();
        auto* view = static_cast<ICoreWebView2*>(webviewView_);
        auto wjson = Utf8ToWide(msg);
        view->PostWebMessageAsJson(wjson.c_str());
        outQueue_.pop();
    }
}

void Overlay::SetMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

} // namespace gui
