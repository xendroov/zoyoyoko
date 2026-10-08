#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <ShlObj.h>

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

// --- Dosya secme dialog ---

std::string OpenFileDialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {};

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
        return std::string(filename);
    return "";
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

    size_t imageSize = nt->OptionalHeader.SizeOfImage;
    void* remoteBase = VirtualAllocEx(hProcess, nullptr, imageSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        printf("[-] VirtualAllocEx basarisiz: 0x%X\n", GetLastError());
        return false;
    }
    printf("[+] Remote base: 0x%p\n", remoteBase);

    WriteProcessMemory(hProcess, remoteBase, dllData.data(),
        nt->OptionalHeader.SizeOfHeaders, nullptr);

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

    auto LoaderShellcode = [](LoaderData* data) -> DWORD {
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(data->ntHeaders);
        auto base = data->imageBase;

        using fnLoadLibraryA = HMODULE(WINAPI*)(const char*);
        using fnGetProcAddress = FARPROC(WINAPI*)(HMODULE, const char*);
        auto pLoadLib = reinterpret_cast<fnLoadLibraryA>(data->pLoadLibraryA);
        auto pGetProc = reinterpret_cast<fnGetProcAddress>(data->pGetProcAddress);

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

        using fnDllMain = BOOL(WINAPI*)(HMODULE, DWORD, LPVOID);
        auto entryPoint = reinterpret_cast<fnDllMain>(base + nt->OptionalHeader.AddressOfEntryPoint);
        entryPoint(reinterpret_cast<HMODULE>(base), DLL_PROCESS_ATTACH, nullptr);

        return 0;
    };

    void* remoteData = VirtualAllocEx(hProcess, nullptr, sizeof(LoaderData),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteData) {
        printf("[-] Loader data alloc basarisiz\n");
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        return false;
    }
    WriteProcessMemory(hProcess, remoteData, &loaderData, sizeof(LoaderData), nullptr);

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

    VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);

    printf("[+] DllMain cagrildi\n");
    return true;
}

// --- LoadLibrary injection (yedek yontem) ---

bool InjectLoadLibrary(HANDLE hProcess, const std::string& dllPath) {
    size_t pathLen = dllPath.size() + 1;

    void* remotePath = VirtualAllocEx(hProcess, nullptr, pathLen,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        printf("[-] VirtualAllocEx basarisiz: 0x%X\n", GetLastError());
        return false;
    }

    WriteProcessMemory(hProcess, remotePath, dllPath.c_str(), pathLen, nullptr);

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    auto pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(hKernel, "LoadLibraryA"));

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        pLoadLibrary, remotePath, 0, nullptr);

    if (!hThread) {
        printf("[-] CreateRemoteThread basarisiz: 0x%X\n", GetLastError());
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD exitCode;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);

    printf("[+] LoadLibrary dondu, HMODULE: 0x%08X\n", exitCode);
    return exitCode != 0;
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
    printf("[+] Debug privilege aktif\n");

    // DLL yolu (kendi yanindaki)
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of('\\') + 1);
    std::string dllPath = exeDir + "KoxpPayload.dll";

    printf("[*] DLL: %s\n", dllPath.c_str());

    auto dllData = ReadFileBytes(dllPath);
    if (dllData.empty()) {
        printf("[-] DLL okunamadi: %s\n", dllPath.c_str());
        system("pause");
        return 1;
    }
    printf("[+] DLL boyutu: %zu bytes\n", dllData.size());

    // Injection yontemi sec
    printf("\n[?] Injection yontemi secin:\n");
    printf("  1 - Otomatik (oyun zaten acik, PID'yi bul)\n");
    printf("  2 - Launcher ile baslat (Launcher.exe yolunu sec)\n");
    printf("  3 - LoadLibrary injection (ManualMap basarisiz olursa)\n");
    printf("\nSeciminiz (1/2/3): ");

    int choice = 0;
    scanf_s("%d", &choice);

    const char* targetProcess = "KnightOnLine.exe";
    DWORD pid = 0;

    if (choice == 2) {
        // Launcher yolunu sec
        printf("\n[*] Launcher.exe dosyasini secin...\n");
        std::string launcherPath = OpenFileDialog(
            "Launcher (*.exe)\0*.exe\0Tum Dosyalar\0*.*\0",
            "Knight Online Launcher Secin");

        if (launcherPath.empty()) {
            printf("[-] Launcher secilmedi\n");
            system("pause");
            return 1;
        }

        printf("[+] Launcher: %s\n", launcherPath.c_str());

        // Launcher'in dizinini al (calisma dizini olarak)
        std::string launcherDir = launcherPath.substr(0, launcherPath.find_last_of('\\'));

        // Launcher'i baslat
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        if (!CreateProcessA(launcherPath.c_str(), nullptr, nullptr, nullptr, FALSE,
            0, nullptr, launcherDir.c_str(), &si, &pi)) {
            printf("[-] Launcher baslatilamadi: 0x%X\n", GetLastError());
            system("pause");
            return 1;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        printf("[+] Launcher baslatildi\n");

        // KnightOnLine.exe'nin acilmasini bekle
        printf("[*] %s bekleniyor (Launcher'dan oyuna girin)...\n", targetProcess);
        for (int i = 0; i < 120 && !pid; ++i) {
            pid = FindProcess(targetProcess);
            if (!pid) {
                printf("\r[*] Bekleniyor... (%d/120s)", i + 1);
                Sleep(1000);
            }
        }
    } else {
        // Mevcut process'i bul
        printf("\n[*] %s araniliyor...\n", targetProcess);
        for (int i = 0; i < 30 && !pid; ++i) {
            pid = FindProcess(targetProcess);
            if (!pid) {
                printf("\r[*] Bekleniyor... (%d/30)", i + 1);
                Sleep(1000);
            }
        }
    }

    if (!pid) {
        printf("\n[-] %s bulunamadi\n", targetProcess);
        system("pause");
        return 1;
    }
    printf("\n[+] PID: %d\n", pid);

    // XIGNCODE yuzunden biraz bekle (tarama bitmeli)
    printf("[*] Anti-cheat taramasi bitmesi icin 5 saniye bekleniyor...\n");
    Sleep(5000);

    // Process'i ac - farkli erisim haklari dene
    HANDLE hProcess = nullptr;

    // Once tam erisim dene
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[*] PROCESS_ALL_ACCESS basarisiz, sinirli haklarla deneniyor...\n");
        hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
            FALSE, pid);
    }

    if (!hProcess) {
        printf("[-] OpenProcess basarisiz: 0x%X\n", GetLastError());
        printf("[-] XIGNCODE process'i koruyor olabilir.\n");
        printf("[-] Launcher ile baslatip, oyun yuklendikten sonra tekrar deneyin.\n");
        system("pause");
        return 1;
    }

    printf("[+] Process acildi\n");

    // Inject
    bool success = false;
    if (choice == 3) {
        printf("\n[*] LoadLibrary injection deneniyor...\n");
        success = InjectLoadLibrary(hProcess, dllPath);
    } else {
        printf("\n[*] ManualMap injection deneniyor...\n");
        success = ManualMap(hProcess, dllData);
        if (!success) {
            printf("[*] ManualMap basarisiz, LoadLibrary ile deneniyor...\n");
            success = InjectLoadLibrary(hProcess, dllPath);
        }
    }

    if (success) {
        printf("\n[+] Injection basarili!\n");
    } else {
        printf("\n[-] Injection basarisiz!\n");
    }

    CloseHandle(hProcess);
    printf("\nKapatmak icin bir tusa basin...\n");
    system("pause");
    return 0;
}
