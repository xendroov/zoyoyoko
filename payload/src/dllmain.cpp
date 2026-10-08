#include <Windows.h>
#include <thread>
#include "core/memory.h"
#include "core/hooks.h"
#include "core/game.h"
#include "core/packets.h"
#include "bot/engine.h"
#include "gui/overlay.h"
#include "gui/bridge.h"

static HMODULE g_hModule = nullptr;
static bool    g_running = false;

// Send hook detour
static void* g_originalSend = nullptr;

void __cdecl HookedSend(const uint8_t* data, int size) {
    core::PacketHook::Get().OnSend(const_cast<uint8_t*>(data), static_cast<size_t>(size));

    // Orijinal send fonksiyonunu cagir
    if (g_originalSend) {
        using OriginalSend = void(__cdecl*)(const uint8_t*, int);
        reinterpret_cast<OriginalSend>(g_originalSend)(data, size);
    }
}

void MainThread(HMODULE hModule) {
    // Konsol olustur (debug icin)
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    printf("[KOXP] Bot yukleniyor...\n");

    // Oyun yuklenene kadar bekle
    Sleep(3000);

    // 1. Memory pointer'larini bul
    auto& game = core::Game::Get();
    if (!game.Initialize()) {
        printf("[KOXP] HATA: Game pointer'lari bulunamadi!\n");
        printf("[KOXP] Pattern'leri sunucu versiyonunuza gore guncelleyin.\n");

        auto& ptrs = game.GetPointers();
        printf("  ptrChar:    0x%08X\n", static_cast<unsigned>(ptrs.ptrChar));
        printf("  fncSend:    0x%08X\n", static_cast<unsigned>(ptrs.fncSend));
        printf("  fncRecv:    0x%08X\n", static_cast<unsigned>(ptrs.fncRecv));
        printf("  fncTarget:  0x%08X\n", static_cast<unsigned>(ptrs.fncTargetSelect));
    } else {
        auto& ptrs = game.GetPointers();
        printf("[KOXP] Pointer'lar bulundu:\n");
        printf("  ptrChar:    0x%08X\n", static_cast<unsigned>(ptrs.ptrChar));
        printf("  fncSend:    0x%08X\n", static_cast<unsigned>(ptrs.fncSend));
        printf("  fncRecv:    0x%08X\n", static_cast<unsigned>(ptrs.fncRecv));
        printf("  fncTarget:  0x%08X\n", static_cast<unsigned>(ptrs.fncTargetSelect));

        // 2. Packet hook'larini kur
        auto& hooks = core::HookManager::Get();
        if (ptrs.fncSend) {
            if (hooks.Install("send", ptrs.fncSend, reinterpret_cast<uintptr_t>(&HookedSend))) {
                g_originalSend = reinterpret_cast<void*>(hooks.GetOriginal<void*>("send"));
                printf("[KOXP] Send hook kuruldu\n");
            }
        }
    }

    // 3. GUI baslat
    printf("[KOXP] GUI baslatiliyor...\n");
    auto& overlay = gui::Overlay::Get();
    overlay.Initialize(hModule);

    // 4. Bridge baslat
    gui::Bridge::Get().Initialize();

    printf("[KOXP] Bot hazir!\n");
    printf("[KOXP] Kapatmak icin 'END' tusuna basin.\n");

    // Ana mesaj dongusu
    g_running = true;
    MSG msg;
    while (g_running) {
        // Pencere mesajlarini isle
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }

        // END tusu ile cikis
        if (GetAsyncKeyState(VK_END) & 1) {
            g_running = false;
        }

        // Durum guncellemelerini GUI'ye gonder (saniyede ~2 kez)
        static DWORD lastUpdate = 0;
        DWORD now = GetTickCount();
        if (now - lastUpdate > 500) {
            gui::Bridge::Get().SendPlayerState();
            gui::Bridge::Get().SendSystemState();
            lastUpdate = now;
        }

        Sleep(16); // ~60 FPS
    }

    // Temizlik
    printf("[KOXP] Kapatiliyor...\n");
    bot::Engine::Get().Stop();
    core::HookManager::Get().RemoveAll();
    overlay.Shutdown();

    fclose(f);
    FreeConsole();
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
