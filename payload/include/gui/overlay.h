#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include <mutex>
#include <queue>

namespace gui {

using MessageHandler = std::function<void(const std::string& message)>;

class Overlay {
public:
    static Overlay& Get();

    bool Initialize(HMODULE hModule);
    void Shutdown();
    bool IsReady() const { return ready_; }

    // C++ -> JS mesaj gonder
    void PostMessage(const std::string& json);

    // JS -> C++ mesaj handler
    void SetMessageHandler(MessageHandler handler);

    HWND GetWindow() const { return hwnd_; }

private:
    Overlay() = default;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void CreateOverlayWindow(HMODULE hModule);
    void InitWebView();
    void ProcessMessageQueue();

    HWND   hwnd_ = nullptr;
    bool   ready_ = false;

    MessageHandler messageHandler_;
    std::mutex     queueMutex_;
    std::queue<std::string> outQueue_;

    // WebView2 pointer'lari (void* olarak, header bagimliligini azaltmak icin)
    void* webviewController_ = nullptr;
    void* webviewView_ = nullptr;
};

} // namespace gui
