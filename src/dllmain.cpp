#include "life.h"
#include "player.h"
#include "arts.h"
#include "i18n.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>

static HMODULE g_self = nullptr;

// Precise Deflect Flash chain-loads this DLL and then asks it for DirectInput.
// Forward that to the system dinput8 so the game still sees the real one.
extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE instance, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    static Fn real = nullptr;
    if (!real) {
        wchar_t path[MAX_PATH];
        const UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (n == 0 || n + 14 >= MAX_PATH) return E_FAIL;
        wcscat_s(path, L"\\dinput8.dll");
        HMODULE mod = LoadLibraryW(path);
        if (!mod) return E_FAIL;
        real = reinterpret_cast<Fn>(GetProcAddress(mod, "DirectInput8Create"));
        if (!real) return E_FAIL;
    }
    return real(instance, version, riid, out, outer);
}

static DWORD WINAPI Worker(LPVOID) {
    LogBind(g_self);
    I18nLoad();
    LifeLoad();
    ArtsLoad();
    Log(T("loaded"));
    Log(T("insert"));
    for (;;) {
        // Rewards and combat-art calls stay off the render thread. The menu is drawn by SekiroPerf after the upscale.
        Sleep(16);
        PlayerFlushGourd();
        LifeTick();
        ArtsTick();
        LifeSettings* s = LifeGet();
        if (s->infiniteHp || s->lifesteal || s->gourd) LifeEnsureWatch();
        if (s->lifesteal || s->gourd) LifeEnsureGuard();
    }
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = self;
        DisableThreadLibraryCalls(self);
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
