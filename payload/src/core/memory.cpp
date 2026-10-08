#include "core/memory.h"
#include <TlHelp32.h>
#include <sstream>
#include <algorithm>

namespace core {

uintptr_t Memory::GetModuleBase(const char* moduleName) {
    if (!moduleName)
        return reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
}

size_t Memory::GetModuleSize(const char* moduleName) {
    HMODULE hModule = moduleName ? GetModuleHandleA(moduleName) : GetModuleHandleA(nullptr);
    if (!hModule) return 0;

    MODULEINFO info{};
    if (GetModuleInformation(GetCurrentProcess(), hModule, &info, sizeof(info)))
        return info.SizeOfImage;

    // Fallback: PE header'dan oku
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uintptr_t>(hModule) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->OptionalHeader.SizeOfImage;
}

std::vector<uint8_t> Memory::ParsePattern(const char* pattern, std::vector<bool>& mask) {
    std::vector<uint8_t> bytes;
    mask.clear();

    std::string pat(pattern);
    std::istringstream stream(pat);
    std::string token;

    while (stream >> token) {
        if (token == "?" || token == "??") {
            bytes.push_back(0);
            mask.push_back(false);
        } else {
            bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
            mask.push_back(true);
        }
    }
    return bytes;
}

uintptr_t Memory::PatternScan(uintptr_t start, size_t size, const char* pattern) {
    std::vector<bool> mask;
    auto bytes = ParsePattern(pattern, mask);
    if (bytes.empty()) return 0;

    auto* scanStart = reinterpret_cast<uint8_t*>(start);
    size_t patternLen = bytes.size();

    for (size_t i = 0; i <= size - patternLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (mask[j] && scanStart[i + j] != bytes[j]) {
                found = false;
                break;
            }
        }
        if (found)
            return start + i;
    }
    return 0;
}

uintptr_t Memory::PatternScan(const char* moduleName, const char* pattern) {
    uintptr_t base = GetModuleBase(moduleName);
    size_t size = GetModuleSize(moduleName);
    if (!base || !size) return 0;
    return PatternScan(base, size, pattern);
}

bool Memory::ReadBytes(uintptr_t address, void* buffer, size_t size) {
    if (IsBadReadPtr(reinterpret_cast<void*>(address), size))
        return false;
    memcpy(buffer, reinterpret_cast<void*>(address), size);
    return true;
}

bool Memory::WriteBytes(uintptr_t address, const void* buffer, size_t size) {
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    memcpy(reinterpret_cast<void*>(address), buffer, size);
    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
    return true;
}

uintptr_t Memory::FollowPointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets) {
    uintptr_t addr = base;
    for (size_t i = 0; i < offsets.size(); ++i) {
        if (IsBadReadPtr(reinterpret_cast<void*>(addr), sizeof(uintptr_t)))
            return 0;
        addr = *reinterpret_cast<uintptr_t*>(addr);
        if (!addr) return 0;
        addr += offsets[i];
    }
    return addr;
}

} // namespace core
