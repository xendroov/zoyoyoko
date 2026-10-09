#include <Windows.h>
#include <thread>
#include <cstdio>
#include <fstream>
#include "core/memory.h"
#include "core/hooks.h"
#include "core/game.h"
#include "core/packets.h"
#include "bot/engine.h"
#include "gui/overlay.h"
#include "gui/bridge.h"

static HMODULE g_hModule = nullptr;
static bool    g_running = false;
static std::ofstream g_logFile;

static void Log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_logFile.is_open()) {
        g_logFile << buf << std::endl;
        g_logFile.flush();
    }
    OutputDebugStringA(buf);
}

static std::string GetDllDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(g_hModule, path, MAX_PATH);
    std::string dir(path);
    auto pos = dir.find_last_of('\\');
    return (pos != std::string::npos) ? dir.substr(0, pos + 1) : dir;
}

void MainThread(HMODULE hModule) {
    std::string logPath = GetDllDirectory() + "m2bot_log.txt";
    g_logFile.open(logPath);

    Log("[M2BOT] Bot yukleniyor...");

    Sleep(3000);

    auto& game = core::Game::Get();
    if (!game.Initialize()) {
        Log("[M2BOT] HATA: Game pointer'lari bulunamadi!");
        Log("[M2BOT] Offsetleri CE ile bulup game.h'a girin.");

        auto& ptrs = game.GetPointers();
        Log("  ptrPlayer:  0x%08X", static_cast<unsigned>(ptrs.ptrPlayer));
        Log("  ptrNetwork: 0x%08X", static_cast<unsigned>(ptrs.ptrNetwork));
        Log("  ptrCharMgr: 0x%08X", static_cast<unsigned>(ptrs.ptrCharMgr));
        Log("  fncSend:    0x%08X", static_cast<unsigned>(ptrs.fncSend));
    } else {
        auto& ptrs = game.GetPointers();
        Log("[M2BOT] Pointer'lar bulundu:");
        Log("  ptrPlayer:  0x%08X", static_cast<unsigned>(ptrs.ptrPlayer));
        Log("  ptrNetwork: 0x%08X", static_cast<unsigned>(ptrs.ptrNetwork));
        Log("  ptrCharMgr: 0x%08X", static_cast<unsigned>(ptrs.ptrCharMgr));
        Log("  fncSend:    0x%08X", static_cast<unsigned>(ptrs.fncSend));
    }

    Log("[M2BOT] GUI baslatiliyor...");
    auto& overlay = gui::Overlay::Get();
    overlay.Initialize(hModule);

    gui::Bridge::Get().Initialize();

    Log("[M2BOT] Bot hazir! Kapatmak icin END tusuna basin.");

    g_running = true;
    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }

        if (GetAsyncKeyState(VK_END) & 1) {
            g_running = false;
        }

        static DWORD lastUpdate = 0;
        DWORD now = GetTickCount();
        if (now - lastUpdate > 500) {
            gui::Bridge::Get().SendPlayerState();
            gui::Bridge::Get().SendSystemState();
            lastUpdate = now;
        }

        Sleep(16);
    }

    Log("[M2BOT] Kapatiliyor...");
    bot::Engine::Get().Stop();
    core::HookManager::Get().RemoveAll();
    overlay.Shutdown();

    g_logFile.close();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        std::thread(MainThread, hModule).detach();
    }
    return TRUE;
}
