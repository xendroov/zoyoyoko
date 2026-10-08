#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace core {

class Memory {
public:
    static uintptr_t GetModuleBase(const char* moduleName = nullptr);
    static size_t    GetModuleSize(const char* moduleName = nullptr);

    // Pattern scan: "48 8B 05 ?? ?? ?? ??" formatinda
    static uintptr_t PatternScan(uintptr_t start, size_t size, const char* pattern);
    static uintptr_t PatternScan(const char* moduleName, const char* pattern);

    template<typename T>
    static T Read(uintptr_t address) {
        if (IsBadReadPtr(reinterpret_cast<void*>(address), sizeof(T)))
            return T{};
        return *reinterpret_cast<T*>(address);
    }

    template<typename T>
    static bool Write(uintptr_t address, T value) {
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        *reinterpret_cast<T*>(address) = value;
        VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), oldProtect, &oldProtect);
        return true;
    }

    static bool ReadBytes(uintptr_t address, void* buffer, size_t size);
    static bool WriteBytes(uintptr_t address, const void* buffer, size_t size);

    // Pointer chain takibi: base -> [base+off1] -> [prev+off2] -> ...
    static uintptr_t FollowPointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets);

private:
    static std::vector<uint8_t> ParsePattern(const char* pattern, std::vector<bool>& mask);
};

} // namespace core
