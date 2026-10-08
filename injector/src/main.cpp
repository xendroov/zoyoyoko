#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

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

// --- Process yolunu al ---

std::string GetProcessPath(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return "";

    char path[MAX_PATH];
    DWORD pathSize = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
        CloseHandle(hProcess);
        return std::string(path);
    }
    CloseHandle(hProcess);
    return "";
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

// --- LoadLibrary injection ---

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

// --- CREATE_SUSPENDED injection (Xigncode oncesi) ---

bool InjectSuspended(const std::string& launcherPath, const std::string& dllPath) {
    std::string launcherDir = launcherPath.substr(0, launcherPath.find_last_of('\\'));

    // 1. Launcher'i baslat
    STARTUPINFOA launcherSi{};
    launcherSi.cb = sizeof(launcherSi);
    PROCESS_INFORMATION launcherPi{};

    if (!CreateProcessA(launcherPath.c_str(), nullptr, nullptr, nullptr, FALSE,
        0, nullptr, launcherDir.c_str(), &launcherSi, &launcherPi)) {
        printf("[-] Launcher baslatilamadi: 0x%X\n", GetLastError());
        return false;
    }
    printf("[+] Launcher baslatildi (PID: %d)\n", launcherPi.dwProcessId);
    CloseHandle(launcherPi.hThread);

    // 2. KnightOnLine.exe'nin Launcher'dan acilmasini bekle
    const char* targetProcess = "KnightOnLine.exe";
    printf("[*] %s bekleniyor (Launcher'dan oyuna girin)...\n", targetProcess);

    DWORD gamePid = 0;
    for (int i = 0; i < 120 && !gamePid; ++i) {
        gamePid = FindProcess(targetProcess);
        if (!gamePid) {
            printf("\r[*] Bekleniyor... (%d/120s)", i + 1);
            Sleep(1000);
        }
    }

    if (!gamePid) {
        printf("\n[-] %s bulunamadi, zaman asimi\n", targetProcess);
        TerminateProcess(launcherPi.hProcess, 0);
        CloseHandle(launcherPi.hProcess);
        return false;
    }

    // 3. Oyun exe yolunu al ve Launcher'in baslattigini kapat
    std::string gamePath = GetProcessPath(gamePid);
    if (gamePath.empty()) {
        gamePath = launcherDir + "\\" + targetProcess;
    }
    printf("\n[+] Oyun yolu: %s\n", gamePath.c_str());

    std::string gameDir = gamePath.substr(0, gamePath.find_last_of('\\'));

    HANDLE hOldGame = OpenProcess(PROCESS_TERMINATE, FALSE, gamePid);
    if (hOldGame) {
        TerminateProcess(hOldGame, 0);
        CloseHandle(hOldGame);
        printf("[+] Launcher'in baslatigi oyun kapatildi\n");
        Sleep(1000);
    }

    // 4. Parent process spoofing icin attribute list hazirla
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);

    std::vector<uint8_t> attrListBuf(attrListSize);
    auto attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrListBuf.data());

    if (!InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize)) {
        printf("[-] Attribute list olusturulamadi: 0x%X\n", GetLastError());
        TerminateProcess(launcherPi.hProcess, 0);
        CloseHandle(launcherPi.hProcess);
        return false;
    }

    if (!UpdateProcThreadAttribute(attrList, 0,
        PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
        &launcherPi.hProcess, sizeof(HANDLE), nullptr, nullptr)) {
        printf("[-] Parent process ayarlanamadi: 0x%X\n", GetLastError());
        DeleteProcThreadAttributeList(attrList);
        TerminateProcess(launcherPi.hProcess, 0);
        CloseHandle(launcherPi.hProcess);
        return false;
    }

    // 5. KnightOnLine.exe'yi CREATE_SUSPENDED ile baslat (parent = Launcher)
    STARTUPINFOEXA siex{};
    siex.StartupInfo.cb = sizeof(siex);
    siex.lpAttributeList = attrList;

    PROCESS_INFORMATION gamePi{};
    if (!CreateProcessA(gamePath.c_str(), nullptr, nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, gameDir.c_str(),
        &siex.StartupInfo, &gamePi)) {
        printf("[-] Oyun CREATE_SUSPENDED ile baslatilamadi: 0x%X\n", GetLastError());
        DeleteProcThreadAttributeList(attrList);
        TerminateProcess(launcherPi.hProcess, 0);
        CloseHandle(launcherPi.hProcess);
        return false;
    }

    printf("[+] Oyun SUSPENDED baslatildi (PID: %d)\n", gamePi.dwProcessId);
    DeleteProcThreadAttributeList(attrList);

    // 6. Suspend durumundayken DLL inject et
    printf("[*] DLL inject ediliyor (suspended)...\n");
    bool injected = InjectLoadLibrary(gamePi.hProcess, dllPath);

    if (!injected) {
        printf("[-] Injection basarisiz, process kapatiliyor\n");
        TerminateProcess(gamePi.hProcess, 0);
        CloseHandle(gamePi.hProcess);
        CloseHandle(gamePi.hThread);
        TerminateProcess(launcherPi.hProcess, 0);
        CloseHandle(launcherPi.hProcess);
        return false;
    }

    // 7. Ana thread'i devam ettir
    ResumeThread(gamePi.hThread);
    printf("[+] Thread resume edildi, oyun basliyor\n");

    CloseHandle(gamePi.hProcess);
    CloseHandle(gamePi.hThread);

    // 8. 6 saniye sonra Launcher'i kapat
    printf("[*] 6 saniye sonra Launcher kapatilacak...\n");
    Sleep(6000);
    TerminateProcess(launcherPi.hProcess, 0);
    CloseHandle(launcherPi.hProcess);
    printf("[+] Launcher kapatildi\n");

    return true;
}

// --- Ana giris ---

int main() {
    printf("=== KOXP Injector (x86) ===\n\n");

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

    printf("\n[?] Injection yontemi secin:\n");
    printf("  1 - Otomatik (oyun zaten acik, PID'yi bul)\n");
    printf("  2 - CREATE_SUSPENDED (Launcher ile, Xigncode oncesi) [ONERILEN]\n");
    printf("  3 - LoadLibrary (oyun acikken, basit injection)\n");
    printf("\nSeciminiz (1/2/3): ");

    int choice = 0;
    scanf_s("%d", &choice);

    if (choice == 2) {
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

        bool success = InjectSuspended(launcherPath, dllPath);
        if (success)
            printf("\n[+] Injection basarili!\n");
        else
            printf("\n[-] Injection basarisiz!\n");

        printf("\nKapatmak icin bir tusa basin...\n");
        system("pause");
        return success ? 0 : 1;
    }

    // Option 1 ve 3: mevcut process'e inject
    const char* targetProcess = "KnightOnLine.exe";
    DWORD pid = 0;

    printf("\n[*] %s araniliyor...\n", targetProcess);
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
        printf("[-] Yontem 2'yi (CREATE_SUSPENDED) deneyin.\n");
        system("pause");
        return 1;
    }

    printf("[+] Process acildi\n");

    printf("\n[*] LoadLibrary injection deneniyor...\n");
    bool success = InjectLoadLibrary(hProcess, dllPath);

    if (success)
        printf("\n[+] Injection basarili!\n");
    else
        printf("\n[-] Injection basarisiz!\n");

    CloseHandle(hProcess);
    printf("\nKapatmak icin bir tusa basin...\n");
    system("pause");
    return success ? 0 : 1;
}
