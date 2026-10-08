#pragma once
#include <Windows.h>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace core {

// x86 inline hook (5-byte JMP)
struct HookContext {
    uintptr_t target;
    uintptr_t detour;
    uint8_t   original[16];
    size_t    patchSize;
    uintptr_t trampoline;
    bool      active;
};

class HookManager {
public:
    static HookManager& Get();

    bool Install(const char* name, uintptr_t target, uintptr_t detour);
    bool Remove(const char* name);
    void RemoveAll();

    HookContext* Find(const char* name);

    // Trampoline: orijinal fonksiyonu cagirmak icin
    template<typename Fn>
    Fn GetOriginal(const char* name) {
        auto* ctx = Find(name);
        if (!ctx || !ctx->trampoline) return nullptr;
        return reinterpret_cast<Fn>(ctx->trampoline);
    }

private:
    HookManager() = default;
    ~HookManager();

    uintptr_t AllocateTrampoline(uintptr_t target, const uint8_t* original, size_t size);
    size_t    CalculatePatchSize(uintptr_t target);

    std::unordered_map<std::string, HookContext> hooks_;
    std::mutex mutex_;
};

} // namespace core
