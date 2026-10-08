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

// Send hook detour (__thiscall -> __fastcall: ECX=this, EDX=unused)
static void* g_originalSend = nullptr;

void __fastcall HookedSend(void* pThis, void* /*edx*/, const uint8_t* data, int size) {
    core::PacketHook::Get().OnSend(const_cast<uint8_t*>(data), static_cast<size_t>(size));

    if (g_originalSend) {
        using OriginalSend = void(__thiscall*)(void*, const uint8_t*, int);
        reinterpret_cast<OriginalSend>(g_originalSend)(pThis, data, size);
    }
}

void MainThread(HMODULE hModule) {
    // Log dosyasi ac (konsol yerine)
    std::string logPath = GetDllDirectory() + "koxp_log.txt";
    g_logFile.open(logPath);

    Log("[KOXP] Bot yukleniyor...");

    // Oyun yuklenene kadar bekle
    Sleep(5000);

    // 1. Memory pointer'larini bul
    auto& game = core::Game::Get();
    if (!game.Initialize()) {
        Log("[KOXP] HATA: Game pointer'lari bulunamadi!");
        Log("[KOXP] Pattern'leri sunucu versiyonunuza gore guncelleyin.");

        auto& ptrs = game.GetPointers();
        Log("  ptrChar:    0x%08X", static_cast<unsigned>(ptrs.ptrChar));
        Log("  fncSend:    0x%08X", static_cast<unsigned>(ptrs.fncSend));
        Log("  fncRecv:    0x%08X", static_cast<unsigned>(ptrs.fncRecv));
        Log("  fncTarget:  0x%08X", static_cast<unsigned>(ptrs.fncTargetSelect));
    } else {
        auto& ptrs = game.GetPointers();
        Log("[KOXP] Pointer'lar bulundu:");
        Log("  ptrChar:    0x%08X", static_cast<unsigned>(ptrs.ptrChar));
        Log("  fncSend:    0x%08X", static_cast<unsigned>(ptrs.fncSend));
        Log("  fncRecv:    0x%08X", static_cast<unsigned>(ptrs.fncRecv));
        Log("  fncTarget:  0x%08X", static_cast<unsigned>(ptrs.fncTargetSelect));

        // 2. Packet hook'larini kur
        auto& hooks = core::HookManager::Get();
        if (ptrs.fncSend) {
            if (hooks.Install("send", ptrs.fncSend, reinterpret_cast<uintptr_t>(&HookedSend))) {
                g_originalSend = reinterpret_cast<void*>(hooks.GetOriginal<void*>("send"));
                Log("[KOXP] Send hook kuruldu");
            }
        }
    }

    // 3. GUI baslat
    Log("[KOXP] GUI baslatiliyor...");
    auto& overlay = gui::Overlay::Get();
    overlay.Initialize(hModule);

    // 4. Bridge baslat
    gui::Bridge::Get().Initialize();

    Log("[KOXP] Bot hazir! Kapatmak icin END tusuna basin.");

    // Ana mesaj dongusu
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

    // Temizlik
    Log("[KOXP] Kapatiliyor...");
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
