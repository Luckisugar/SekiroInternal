#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <stdio.h>
#include <string.h>

static bool FindGame(DWORD* pid, wchar_t* path, int pathChars) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"sekiro.exe") == 0) {
                *pid = pe.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!found) return false;
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *pid);
    if (!proc) return false;
    DWORD n = pathChars;
    BOOL ok = QueryFullProcessImageNameW(proc, 0, path, &n);
    CloseHandle(proc);
    return ok == TRUE;
}

static bool AlreadyIn(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, L"SekiroInternal.dll") == 0) found = true;
        } while (!found && Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

static bool OldLoaderPresent(const wchar_t* exePath) {
    wchar_t dir[MAX_PATH];
    wcscpy_s(dir, exePath);
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (!slash) return false;
    *(slash + 1) = 0;
    wchar_t file[MAX_PATH];
    wcscpy_s(file, dir);
    wcscat_s(file, L"BloodOfTheDragon.dll");
    if (GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES) return true;
    wcscpy_s(file, dir);
    wcscat_s(file, L"winmm.dll");
    return GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES;
}

static bool Inject(DWORD pid, const wchar_t* dll) {
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) {
        Logf("OpenProcess failed (%lu). If Sekiro is running as admin, this injector is not. Not elevating.", GetLastError());
        return false;
    }
    size_t bytes = (wcslen(dll) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        Logf("VirtualAllocEx failed (%lu).", GetLastError());
        CloseHandle(proc);
        return false;
    }
    if (!WriteProcessMemory(proc, remote, dll, bytes, nullptr)) {
        Logf("WriteProcessMemory failed (%lu).", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto load = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "LoadLibraryW");
    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, load, remote, 0, nullptr);
    if (!thread) {
        Logf("CreateRemoteThread failed (%lu).", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    WaitForSingleObject(thread, 8000);
    CloseHandle(thread);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return AlreadyIn(pid);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"SekiroInternal");
    LogBind(nullptr);
    Log("Preflight.");

    wchar_t dll[MAX_PATH];
    GetModuleFileNameW(nullptr, dll, MAX_PATH);
    wchar_t* slash = wcsrchr(dll, L'\\');
    if (!slash) {
        LogFail("Injector path is not readable.");
    } else {
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - dll), L"SekiroInternal.dll");
    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) {
        LogFail("SekiroInternal.dll is not next to Inject.exe.");
    } else {

    DWORD pid = 0;
    wchar_t game[MAX_PATH];
    if (!FindGame(&pid, game, MAX_PATH)) {
        LogFail("sekiro.exe is not running. Start the game, wait for the main menu, then run Inject.exe.");
    } else {
        Log("sekiro.exe is running.");
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if (!GetFileAttributesExW(game, GetFileExInfoStandard, &info) || info.nFileSizeLow != 68005144u || info.nFileSizeHigh != 0) {
            LogFail("sekiro.exe is not the 1.06 file this build knows (68005144 bytes). Not injected.");
        } else if (AlreadyIn(pid)) {
            Log("sekiro.exe size matches patch 1.06.");
            Log("SekiroInternal.dll is already in the game. Press Insert.");
        } else {
            Log("sekiro.exe size matches patch 1.06.");
            if (OldLoaderPresent(game)) {
                LogFail("winmm.dll or BloodOfTheDragon.dll is still beside sekiro.exe. Close the game and delete those two files. This menu will not load on top of that hook.");
            } else {
            Log("Injecting.");
            if (!Inject(pid, dll)) LogFail("Injection did not stick. SekiroInternal.dll is not in the process.");
            else Log("Injected. In game, press Insert.");
            }
        }
    }
    }
    }
    printf("Press Enter to close.\n");
    getchar();
    return 0;
}
