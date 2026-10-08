#include "gui/overlay.h"

// WebView2 header'lari - NuGet'ten veya SDK'dan
// Derleme sirasinda WebView2 SDK kurulu olmali
// #include <wrl.h>
// #include <WebView2.h>
// #include <WebView2EnvironmentOptions.h>

namespace gui {

Overlay& Overlay::Get() {
    static Overlay instance;
    return instance;
}

static const wchar_t* WINDOW_CLASS = L"KoxpOverlay";
static const wchar_t* WINDOW_TITLE = L"KOXP Bot";

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: {
            auto* self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (self && self->webviewController_) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                // controller->put_Bounds(bounds);
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
    wc.hbrBackground  = CreateSolidBrush(RGB(15, 15, 35)); // koyu arka plan
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
    /*
    WebView2 baslangic kodu:

    CreateCoreWebView2EnvironmentWithOptions kullanilir.
    Asagidaki pseudo-code gercek implementasyonu gosterir:

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                env->CreateCoreWebView2Controller(hwnd_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            webviewController_ = controller;
                            controller->get_CoreWebView2((ICoreWebView2**)&webviewView_);

                            auto* view = (ICoreWebView2*)webviewView_;

                            // Pencere boyutunu ayarla
                            RECT bounds;
                            GetClientRect(hwnd_, &bounds);
                            controller->put_Bounds(bounds);

                            // HTML icerigini yukle
                            view->NavigateToString(GetEmbeddedHtml());

                            // JS -> C++ mesaj handler
                            view->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR msg;
                                        args->TryGetWebMessageAsString(&msg);
                                        // UTF-16 -> UTF-8 donusumu
                                        // messageHandler_(utf8msg);
                                        CoTaskMemFree(msg);
                                        return S_OK;
                                    }).Get(), nullptr);

                            ready_ = true;
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
    */
}

bool Overlay::Initialize(HMODULE hModule) {
    CreateOverlayWindow(hModule);
    InitWebView();
    return hwnd_ != nullptr;
}

void Overlay::Shutdown() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(WINDOW_CLASS, nullptr);
    ready_ = false;
}

void Overlay::PostMessage(const std::string& json) {
    if (!ready_ || !webviewView_) return;

    // auto* view = (ICoreWebView2*)webviewView_;
    // wstring wjson = utf8_to_wstring(json);
    // view->PostWebMessageAsJson(wjson.c_str());
}

void Overlay::SetMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

} // namespace gui
