#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <string>
#include <fstream>
#include <ShlObj.h>

static const char* TARGET_PROCESS = "metin2client.bin";

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

int main() {
    printf("=== M2Bot Injector (x86) ===\n\n");

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

    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of('\\') + 1);
    std::string dllPath = exeDir + "M2Payload.dll";

    printf("[*] DLL: %s\n", dllPath.c_str());

    {
        std::ifstream check(dllPath, std::ios::binary);
        if (!check.is_open()) {
            printf("[-] DLL bulunamadi: %s\n", dllPath.c_str());
            system("pause");
            return 1;
        }
        check.seekg(0, std::ios::end);
        printf("[+] DLL boyutu: %lld bytes\n", static_cast<long long>(check.tellg()));
    }

    printf("\n[*] %s araniliyor...\n", TARGET_PROCESS);
    printf("[*] Metin2 client'i acin ve giris yapin.\n");

    DWORD pid = 0;
    for (int i = 0; i < 60 && !pid; ++i) {
        pid = FindProcess(TARGET_PROCESS);
        if (!pid) {
            printf("\r[*] Bekleniyor... (%d/60s)", i + 1);
            Sleep(1000);
        }
    }

    if (!pid) {
        printf("\n[-] %s bulunamadi (60s timeout)\n", TARGET_PROCESS);
        system("pause");
        return 1;
    }
    printf("\n[+] %s bulundu (PID: %d)\n", TARGET_PROCESS, pid);

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[*] PROCESS_ALL_ACCESS basarisiz, sinirli haklarla deneniyor...\n");
        hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
            FALSE, pid);
    }

    if (!hProcess) {
        printf("[-] OpenProcess basarisiz: 0x%X\n", GetLastError());
        printf("[-] Metin2'yi yonetici olarak calistirmayin.\n");
        system("pause");
        return 1;
    }

    printf("[+] Process acildi\n");
    printf("[*] LoadLibrary injection yapiliyor...\n");

    bool success = InjectLoadLibrary(hProcess, dllPath);

    if (success)
        printf("\n[+] Injection basarili! Bot yuklendi.\n");
    else
        printf("\n[-] Injection basarisiz!\n");

    CloseHandle(hProcess);
    printf("\nKapatmak icin bir tusa basin...\n");
    system("pause");
    return success ? 0 : 1;
}
