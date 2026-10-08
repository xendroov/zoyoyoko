#include "core/hooks.h"
#include <cstring>

namespace core {

HookManager& HookManager::Get() {
    static HookManager instance;
    return instance;
}

HookManager::~HookManager() {
    RemoveAll();
}

size_t HookManager::CalculatePatchSize(uintptr_t target) {
    // x86 icin minimum 5 byte (JMP rel32)
    // Basit yaklasim: 5 byte yeterli, instruction boundary kontrolu sonra eklenebilir
    return 5;
}

uintptr_t HookManager::AllocateTrampoline(uintptr_t target, const uint8_t* original, size_t size) {
    // Trampoline: orijinal instruction'lar + JMP back
    size_t trampolineSize = size + 5; // original bytes + JMP rel32
    auto* trampoline = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, trampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline) return 0;

    // Orijinal byte'lari kopyala
    memcpy(trampoline, original, size);

    // JMP back: target + size adresine don
    uintptr_t jumpBack = target + size;
    trampoline[size] = 0xE9; // JMP rel32
    *reinterpret_cast<int32_t*>(&trampoline[size + 1]) =
        static_cast<int32_t>(jumpBack - reinterpret_cast<uintptr_t>(&trampoline[size]) - 5);

    return reinterpret_cast<uintptr_t>(trampoline);
}

bool HookManager::Install(const char* name, uintptr_t target, uintptr_t detour) {
    std::lock_guard lock(mutex_);

    if (hooks_.count(name)) return false;

    HookContext ctx{};
    ctx.target = target;
    ctx.detour = detour;
    ctx.patchSize = CalculatePatchSize(target);
    ctx.active = false;

    // Orijinal byte'lari kaydet
    memcpy(ctx.original, reinterpret_cast<void*>(target), ctx.patchSize);

    // Trampoline olustur
    ctx.trampoline = AllocateTrampoline(target, ctx.original, ctx.patchSize);
    if (!ctx.trampoline) return false;

    // Hook yaz: JMP detour
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(target), ctx.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(reinterpret_cast<void*>(ctx.trampoline), 0, MEM_RELEASE);
        return false;
    }

    auto* patch = reinterpret_cast<uint8_t*>(target);
    patch[0] = 0xE9; // JMP rel32
    *reinterpret_cast<int32_t*>(&patch[1]) =
        static_cast<int32_t>(detour - target - 5);

    // Kalan byte'lari NOP ile doldur
    for (size_t i = 5; i < ctx.patchSize; ++i)
        patch[i] = 0x90;

    VirtualProtect(reinterpret_cast<void*>(target), ctx.patchSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target), ctx.patchSize);

    ctx.active = true;
    hooks_[name] = ctx;
    return true;
}

bool HookManager::Remove(const char* name) {
    std::lock_guard lock(mutex_);

    auto it = hooks_.find(name);
    if (it == hooks_.end()) return false;

    auto& ctx = it->second;
    if (ctx.active) {
        DWORD oldProtect;
        VirtualProtect(reinterpret_cast<void*>(ctx.target), ctx.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(reinterpret_cast<void*>(ctx.target), ctx.original, ctx.patchSize);
        VirtualProtect(reinterpret_cast<void*>(ctx.target), ctx.patchSize, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(ctx.target), ctx.patchSize);
    }

    if (ctx.trampoline)
        VirtualFree(reinterpret_cast<void*>(ctx.trampoline), 0, MEM_RELEASE);

    hooks_.erase(it);
    return true;
}

void HookManager::RemoveAll() {
    std::lock_guard lock(mutex_);
    for (auto& [name, ctx] : hooks_) {
        if (ctx.active) {
            DWORD oldProtect;
            VirtualProtect(reinterpret_cast<void*>(ctx.target), ctx.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy(reinterpret_cast<void*>(ctx.target), ctx.original, ctx.patchSize);
            VirtualProtect(reinterpret_cast<void*>(ctx.target), ctx.patchSize, oldProtect, &oldProtect);
        }
        if (ctx.trampoline)
            VirtualFree(reinterpret_cast<void*>(ctx.trampoline), 0, MEM_RELEASE);
    }
    hooks_.clear();
}

HookContext* HookManager::Find(const char* name) {
    std::lock_guard lock(mutex_);
    auto it = hooks_.find(name);
    return it != hooks_.end() ? &it->second : nullptr;
}

} // namespace core
