#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static HMODULE g_mod = nullptr;
static wchar_t g_dir[MAX_PATH];
static wchar_t g_log[MAX_PATH];
static bool g_ready = false;
static CRITICAL_SECTION g_cs;
static bool g_csInit = false;

static const int kLines = 40;
static const int kLine = 180;
static char g_ring[kLines][kLine];
static int g_ringN = 0;

static void Ensure() {
    if (!g_csInit) {
        InitializeCriticalSection(&g_cs);
        g_csInit = true;
    }
    if (g_ready) return;
    GetModuleFileNameW(g_mod, g_dir, MAX_PATH);
    wchar_t* slash = wcsrchr(g_dir, L'\\');
    if (slash) *(slash + 1) = 0;
    else g_dir[0] = 0;
    wcscpy_s(g_log, g_dir);
    wcscat_s(g_log, L"activity.log");
    g_ready = true;
}

void LogBind(HMODULE module) {
    g_mod = module;
    g_ready = false;
    Ensure();
}

const wchar_t* LogDir() {
    Ensure();
    return g_dir;
}

static void WriteLine(const char* text) {
    Ensure();
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char line[512];
    sprintf_s(line, "[%02d:%02d:%02d] %s\r\n", st.wHour, st.wMinute, st.wSecond, text);

    EnterCriticalSection(&g_cs);
    int slot = g_ringN % kLines;
    strncpy_s(g_ring[slot], line, _TRUNCATE);
    g_ringN += 1;
    HANDLE file = CreateFileW(g_log, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        WriteFile(file, line, (DWORD)strlen(line), &wrote, nullptr);
        CloseHandle(file);
    }
    LeaveCriticalSection(&g_cs);

    HANDLE con = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (con && con != INVALID_HANDLE_VALUE && GetConsoleMode(con, &mode)) {
        DWORD wrote = 0;
        WriteConsoleA(con, line, (DWORD)strlen(line), &wrote, nullptr);
    }
}

void Log(const char* line) {
    if (!line) return;
    WriteLine(line);
}

void Logf(const char* fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsprintf_s(buf, fmt, ap);
    va_end(ap);
    WriteLine(buf);
}

void LogFail(const char* line) {
    Log(line);
    Ensure();
    wchar_t path[MAX_PATH];
    wcscpy_s(path, g_dir);
    wcscat_s(path, L"last-error.txt");
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(file, line, (DWORD)strlen(line), &wrote, nullptr);
    CloseHandle(file);
}

int LogCopy(char* dest, int destBytes, int maxLines) {
    Ensure();
    if (!dest || destBytes < 2 || maxLines < 1) return 0;
    EnterCriticalSection(&g_cs);
    int n = g_ringN < kLines ? g_ringN : kLines;
    int start = g_ringN < kLines ? 0 : g_ringN % kLines;
    if (n > maxLines) {
        start = (start + (n - maxLines)) % kLines;
        n = maxLines;
    }
    dest[0] = 0;
    int used = 0;
    for (int i = 0; i < n; ++i) {
        const char* src = g_ring[(start + i) % kLines];
        int len = (int)strlen(src);
        if (used + len + 1 >= destBytes) break;
        memcpy(dest + used, src, len);
        used += len;
        dest[used] = 0;
    }
    LeaveCriticalSection(&g_cs);
    return n;
}
