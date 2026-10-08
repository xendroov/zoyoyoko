#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>

// --- Process bulma ---

DWORD FindProcess(const char* processName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);

    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, processName) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

// --- Admin kontrolu ---

bool IsElevated() {
    BOOL elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev{};
        DWORD size;
        if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size))
            elevated = elev.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated;
}

bool RequestElevation() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    SHELLEXECUTEINFOA sei{};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = path;
    sei.nShow  = SW_SHOWNORMAL;

    if (ShellExecuteExA(&sei)) {
        ExitProcess(0);
        return true;
    }
    return false;
}

// --- Debug privilege ---

bool EnableDebugPrivilege() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &tp.Privileges[0].Luid);

    bool ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(token);
    return ok;
}

// --- DLL okuma ---

std::vector<uint8_t> ReadFileBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

// --- Manual Map Injection ---

bool ManualMap(HANDLE hProcess, const std::vector<uint8_t>& dllData) {
    // 1. PE header'larini parse et
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(dllData.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("[-] Gecersiz DOS signature\n");
        return false;
    }

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(dllData.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        printf("[-] Gecersiz NT signature\n");
        return false;
    }

    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        printf("[-] DLL x86 olmali\n");
        return false;
    }

    // 2. Hedef process'te bellek ayir
    size_t imageSize = nt->OptionalHeader.SizeOfImage;
    void* remoteBase = VirtualAllocEx(hProcess, nullptr, imageSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        printf("[-] VirtualAllocEx basarisiz: 0x%X\n", GetLastError());
        return false;
    }
    printf("[+] Remote base: 0x%p\n", remoteBase);

    // 3. Header'lari yaz
    WriteProcessMemory(hProcess, remoteBase, dllData.data(),
        nt->OptionalHeader.SizeOfHeaders, nullptr);

    // 4. Section'lari yaz
    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (section[i].SizeOfRawData == 0) continue;

        void* dest = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(remoteBase) + section[i].VirtualAddress);
        const void* src = dllData.data() + section[i].PointerToRawData;

        if (!WriteProcessMemory(hProcess, dest, src, section[i].SizeOfRawData, nullptr)) {
            printf("[-] Section yazma basarisiz: %s\n", reinterpret_cast<const char*>(section[i].Name));
        }
    }

    // 5. Shellcode: relocation + import + DllMain cagirma
    // Shellcode icin loader struct
    struct LoaderData {
        uintptr_t imageBase;
        uintptr_t ntHeaders;
        uintptr_t pLoadLibraryA;
        uintptr_t pGetProcAddress;
    };

    LoaderData loaderData{};
    loaderData.imageBase      = reinterpret_cast<uintptr_t>(remoteBase);
    loaderData.ntHeaders      = reinterpret_cast<uintptr_t>(remoteBase) + dos->e_lfanew;
    loaderData.pLoadLibraryA  = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));
    loaderData.pGetProcAddress = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress"));

    // Loader shellcode (C fonksiyonu olarak)
    auto LoaderShellcode = [](LoaderData* data) -> DWORD {
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(data->ntHeaders);
        auto base = data->imageBase;

        using fnLoadLibraryA = HMODULE(WINAPI*)(const char*);
        using fnGetProcAddress = FARPROC(WINAPI*)(HMODULE, const char*);
        auto pLoadLib = reinterpret_cast<fnLoadLibraryA>(data->pLoadLibraryA);
        auto pGetProc = reinterpret_cast<fnGetProcAddress>(data->pGetProcAddress);

        // Relocation
        auto delta = static_cast<ptrdiff_t>(base - nt->OptionalHeader.ImageBase);
        if (delta != 0 && nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0) {
            auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

            while (reloc->VirtualAddress) {
                DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                auto* entry = reinterpret_cast<WORD*>(reinterpret_cast<uintptr_t>(reloc) + sizeof(IMAGE_BASE_RELOCATION));

                for (DWORD i = 0; i < count; ++i) {
                    if ((entry[i] >> 12) == IMAGE_REL_BASED_HIGHLOW) {
                        auto* patch = reinterpret_cast<uintptr_t*>(base + reloc->VirtualAddress + (entry[i] & 0xFFF));
                        *patch += delta;
                    }
                }
                reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                    reinterpret_cast<uintptr_t>(reloc) + reloc->SizeOfBlock);
            }
        }

        // Import resolution
        if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size > 0) {
            auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
                base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

            while (importDesc->Name) {
                auto* dllName = reinterpret_cast<const char*>(base + importDesc->Name);
                HMODULE hDll = pLoadLib(dllName);

                auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + importDesc->FirstThunk);
                auto* origThunk = importDesc->OriginalFirstThunk
                    ? reinterpret_cast<IMAGE_THUNK_DATA*>(base + importDesc->OriginalFirstThunk)
                    : thunk;

                while (origThunk->u1.AddressOfData) {
                    if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                        thunk->u1.Function = reinterpret_cast<uintptr_t>(
                            pGetProc(hDll, reinterpret_cast<const char*>(origThunk->u1.Ordinal & 0xFFFF)));
                    } else {
                        auto* import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + origThunk->u1.AddressOfData);
                        thunk->u1.Function = reinterpret_cast<uintptr_t>(pGetProc(hDll, import->Name));
                    }
                    ++thunk;
                    ++origThunk;
                }
                ++importDesc;
            }
        }

        // DllMain cagir
        using fnDllMain = BOOL(WINAPI*)(HMODULE, DWORD, LPVOID);
        auto entryPoint = reinterpret_cast<fnDllMain>(base + nt->OptionalHeader.AddressOfEntryPoint);
        entryPoint(reinterpret_cast<HMODULE>(base), DLL_PROCESS_ATTACH, nullptr);

        return 0;
    };

    // Shellcode'u ve data'yi hedef process'e yaz
    void* remoteData = VirtualAllocEx(hProcess, nullptr, sizeof(LoaderData),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteData) {
        printf("[-] Loader data alloc basarisiz\n");
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        return false;
    }
    WriteProcessMemory(hProcess, remoteData, &loaderData, sizeof(LoaderData), nullptr);

    // Shellcode boyutunu tahmin et (sayfaya yuvarla)
    size_t shellcodeSize = 4096;
    void* remoteShellcode = VirtualAllocEx(hProcess, nullptr, shellcodeSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteShellcode) {
        printf("[-] Shellcode alloc basarisiz\n");
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
        return false;
    }
    WriteProcessMemory(hProcess, remoteShellcode, &LoaderShellcode, shellcodeSize, nullptr);

    // 6. Remote thread baslat
    printf("[+] Loader thread baslatiliyor...\n");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellcode),
        remoteData, 0, nullptr);

    if (!hThread) {
        printf("[-] CreateRemoteThread basarisiz: 0x%X\n", GetLastError());
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD exitCode;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    // Temizlik
    VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);

    printf("[+] DllMain cagrildi\n");
    return true;
}

// --- Ana giris ---

int main() {
    printf("=== KOXP Injector (x86) ===\n\n");

    // Admin kontrolu
    if (!IsElevated()) {
        printf("[*] Yonetici izni gerekiyor, UAC onayi isteniyor...\n");
        if (!RequestElevation()) {
            printf("[-] Yonetici izni alinamadi\n");
            system("pause");
            return 1;
        }
        return 0;
    }

    EnableDebugPrivilege();

    // DLL yolu
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string dir(exePath);
    dir = dir.substr(0, dir.find_last_of('\\') + 1);
    std::string dllPath = dir + "KoxpPayload.dll";

    printf("[*] DLL: %s\n", dllPath.c_str());

    // DLL oku
    auto dllData = ReadFileBytes(dllPath);
    if (dllData.empty()) {
        printf("[-] DLL okunamadi: %s\n", dllPath.c_str());
        system("pause");
        return 1;
    }
    printf("[+] DLL boyutu: %zu bytes\n", dllData.size());

    // Process bul
    const char* targetProcess = "KnightOnLine.exe";
    printf("[*] %s araniliyor...\n", targetProcess);

    DWORD pid = 0;
    for (int i = 0; i < 30 && !pid; ++i) {
        pid = FindProcess(targetProcess);
        if (!pid) {
            printf("\r[*] Bekleniyor... (%d/30)", i + 1);
            Sleep(1000);
        }
    }

    if (!pid) {
        printf("\n[-] %s bulunamadi\n", targetProcess);
        system("pause");
        return 1;
    }
    printf("\n[+] PID: %d\n", pid);

    // Process'i ac
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[-] OpenProcess basarisiz: 0x%X\n", GetLastError());
        system("pause");
        return 1;
    }

    // Inject
    if (ManualMap(hProcess, dllData)) {
        printf("\n[+] Injection basarili!\n");
    } else {
        printf("\n[-] Injection basarisiz!\n");
    }

    CloseHandle(hProcess);
    printf("\nKapatmak icin bir tusa basin...\n");
    system("pause");
    return 0;
}
