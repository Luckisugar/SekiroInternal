#include "overlay.h"
#include "life.h"
#include "player.h"
#include "i18n.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HMODULE g_self = nullptr;

static DWORD WINAPI Worker(LPVOID) {
    LogBind(g_self);
    I18nLoad();
    LifeLoad();
    Log(T("loaded"));
    Log(T("insert"));
    if (!OverlayInstall()) return 0;
    for (;;) {
        Sleep(400);
        PlayerFlushGourd();
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
