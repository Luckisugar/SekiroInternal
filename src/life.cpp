#include "life.h"
#include "player.h"
#include "config.h"
#include "i18n.h"
#include "log.h"
#include "hde64.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LifeSettings g_life{};
static bool g_loaded = false;
static char g_watch[160] = "not installed";
static bool g_watchOn = false;
static char g_guard[96] = "not installed";
static bool g_guardOn = false;
static int g_paid[ActCount] = {};
static int g_every[ActCount][4] = {};
static float g_gourdAcc[ActCount] = {};
static bool g_statsLogged = false;
static int g_gourdPaid = 0;

static const int kQueueCap = 64;
struct Pending { int action; int maxHp; };
static Pending g_queue[kQueueCap];
static int g_qRead = 0;
static int g_qWrite = 0;
static CRITICAL_SECTION g_qCs;
static bool g_qInit = false;

static uint8_t* g_lastObj = nullptr;
static int g_lastOld = 0;
static int g_lastNew = 0;
static DWORD g_lastTick = 0;
static volatile LONG g_idRejects = 0;
static bool g_idLogged = false;

static Rule DefaultRule(int action) {
    Rule r{};
    r.vitalityEvery = 1;
    r.emblemsEvery = 1;
    r.postureEvery = 1;
    if (action == ActHit) r.vitalityPercent = 1.5f;
    if (action == ActBlock) r.vitalityPercent = 1.f;
    r.gourdEvery = 1;
    if (action == ActParry) {
        r.vitalityPercent = 1.5f;
        r.emblems = 1;
        r.emblemsEvery = 8;
        r.posturePercent = 6.f;
        r.gourdCharges = 1.f;
        r.gourdEvery = 16;
    }
    if (action == ActDeath) {
        r.vitalityPercent = 8.f;
        r.emblems = 1;
        r.posturePercent = 33.f;
        r.gourdCharges = 1.f;
        r.gourdEvery = 3;
    }
    if (action == ActDeathMini) {
        r.vitalityPercent = 10.f;
        r.emblems = 2;
        r.posturePercent = 66.f;
        r.gourdCharges = 1.f;
    }
    if (action == ActDeathBoss) {
        r.vitalityPercent = 12.f;
        r.emblems = 4;
        r.posturePercent = 100.f;
        r.gourdCharges = 1.f;
    }
    return r;
}

static void Defaults(LifeSettings* s) {
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < ActCount; ++i) s->rule[i] = DefaultRule(i);
    s->gourd = true;
    s->blocksPerCharge = 8;
    s->parryIfPostureGainAtMost = 18;
    s->miniBossVitalityAtLeast = 800;
    s->bossVitalityAtLeast = 2200;
    s->gourdChargeCap = 10;
}

static wchar_t* IniPath() {
    static wchar_t path[MAX_PATH];
    wcscpy_s(path, LogDir());
    wcscat_s(path, L"settings.ini");
    return path;
}

static int ClampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static const wchar_t* Section(int action) {
    switch (action) {
    case ActHit: return L"Hit";
    case ActBlock: return L"Block";
    case ActParry: return L"Parry";
    case ActDeath: return L"Deathblow";
    case ActDeathMini: return L"DeathblowMiniBoss";
    case ActDeathBoss: return L"DeathblowBoss";
    default: return L"Hit";
    }
}

static void ClampRule(Rule* r) {
    if (r->vitalityPercent < 0.f) r->vitalityPercent = 0.f;
    if (r->vitalityPercent > 100.f) r->vitalityPercent = 100.f;
    if (r->posturePercent < 0.f) r->posturePercent = 0.f;
    if (r->posturePercent > 100.f) r->posturePercent = 100.f;
    r->emblems = ClampInt(r->emblems, 0, 20);
    r->vitalityEvery = ClampInt(r->vitalityEvery, 1, 100);
    r->emblemsEvery = ClampInt(r->emblemsEvery, 1, 100);
    r->postureEvery = ClampInt(r->postureEvery, 1, 100);
    if (r->gourdCharges < 0.f) r->gourdCharges = 0.f;
    if (r->gourdCharges > 10.f) r->gourdCharges = 10.f;
    r->gourdEvery = ClampInt(r->gourdEvery, 1, 100);
}

LifeSettings* LifeGet() {
    if (!g_loaded) LifeLoad();
    return &g_life;
}

void LifeLoad() {
    Defaults(&g_life);
    wchar_t* ini = IniPath();
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
        g_loaded = true;
        LifeSave();
        return;
    }
    g_life.infiniteHp = GetPrivateProfileIntW(L"wolf", L"infiniteHealth", 0, ini) != 0;
    g_life.undeath = GetPrivateProfileIntW(L"wolf", L"undeath", 0, ini) != 0;
    g_life.infiniteEmblems = GetPrivateProfileIntW(L"wolf", L"infiniteEmblems", 0, ini) != 0;
    g_life.lifesteal = GetPrivateProfileIntW(L"lifesteal", L"enabled", 0, ini) != 0;
    g_life.gourd = GetPrivateProfileIntW(L"gourd", L"enabled", 1, ini) != 0;
    g_life.blocksPerCharge = GetPrivateProfileIntW(L"gourd", L"blocksPerCharge", 8, ini);
    g_life.gourdChargeCap = GetPrivateProfileIntW(L"gourd", L"gourdChargeCap", 10, ini);
    g_life.parryIfPostureGainAtMost = GetPrivateProfileIntW(L"lifesteal", L"parryIfPostureGainAtMost", 18, ini);
    g_life.miniBossVitalityAtLeast = GetPrivateProfileIntW(L"lifesteal", L"miniBossVitalityAtLeast", 800, ini);
    g_life.bossVitalityAtLeast = GetPrivateProfileIntW(L"lifesteal", L"bossVitalityAtLeast", 2200, ini);
    for (int i = 0; i < ActCount; ++i) {
        Rule d = DefaultRule(i);
        const wchar_t* sec = Section(i);
        wchar_t buf[32];
        GetPrivateProfileStringW(sec, L"vitalityPercent", L"", buf, 32, ini);
        g_life.rule[i].vitalityPercent = buf[0] ? (float)_wtof(buf) : d.vitalityPercent;
        g_life.rule[i].vitalityEvery = GetPrivateProfileIntW(sec, L"vitalityEvery", d.vitalityEvery, ini);
        g_life.rule[i].emblems = GetPrivateProfileIntW(sec, L"emblems", d.emblems, ini);
        g_life.rule[i].emblemsEvery = GetPrivateProfileIntW(sec, L"emblemsEvery", d.emblemsEvery, ini);
        GetPrivateProfileStringW(sec, L"posturePercent", L"", buf, 32, ini);
        g_life.rule[i].posturePercent = buf[0] ? (float)_wtof(buf) : d.posturePercent;
        g_life.rule[i].postureEvery = GetPrivateProfileIntW(sec, L"postureEvery", d.postureEvery, ini);
        GetPrivateProfileStringW(sec, L"gourdCharges", L"", buf, 32, ini);
        g_life.rule[i].gourdCharges = buf[0] ? (float)_wtof(buf) : d.gourdCharges;
        g_life.rule[i].gourdEvery = GetPrivateProfileIntW(sec, L"gourdEvery", d.gourdEvery, ini);
        ClampRule(&g_life.rule[i]);
    }
    g_life.blocksPerCharge = ClampInt(g_life.blocksPerCharge, 5, 10);
    g_life.parryIfPostureGainAtMost = ClampInt(g_life.parryIfPostureGainAtMost, 1, 5000);
    g_life.miniBossVitalityAtLeast = ClampInt(g_life.miniBossVitalityAtLeast, 1, 100000);
    g_life.bossVitalityAtLeast = ClampInt(g_life.bossVitalityAtLeast, 1, 100000);
    g_life.gourdChargeCap = ClampInt(g_life.gourdChargeCap, 1, 10);
    g_loaded = true;
    PlayerSetGourdCap(g_life.gourdChargeCap);
}

void LifeSave() {
    wchar_t* ini = IniPath();
    wchar_t num[32];
    swprintf_s(num, L"%d", g_life.infiniteHp ? 1 : 0);
    WritePrivateProfileStringW(L"wolf", L"infiniteHealth", num, ini);
    swprintf_s(num, L"%d", g_life.undeath ? 1 : 0);
    WritePrivateProfileStringW(L"wolf", L"undeath", num, ini);
    swprintf_s(num, L"%d", g_life.infiniteEmblems ? 1 : 0);
    WritePrivateProfileStringW(L"wolf", L"infiniteEmblems", num, ini);
    swprintf_s(num, L"%d", g_life.lifesteal ? 1 : 0);
    WritePrivateProfileStringW(L"lifesteal", L"enabled", num, ini);
    swprintf_s(num, L"%d", g_life.gourd ? 1 : 0);
    WritePrivateProfileStringW(L"gourd", L"enabled", num, ini);
    swprintf_s(num, L"%d", g_life.blocksPerCharge);
    WritePrivateProfileStringW(L"gourd", L"blocksPerCharge", num, ini);
    swprintf_s(num, L"%d", g_life.gourdChargeCap);
    WritePrivateProfileStringW(L"gourd", L"gourdChargeCap", num, ini);
    swprintf_s(num, L"%d", g_life.parryIfPostureGainAtMost);
    WritePrivateProfileStringW(L"lifesteal", L"parryIfPostureGainAtMost", num, ini);
    swprintf_s(num, L"%d", g_life.miniBossVitalityAtLeast);
    WritePrivateProfileStringW(L"lifesteal", L"miniBossVitalityAtLeast", num, ini);
    swprintf_s(num, L"%d", g_life.bossVitalityAtLeast);
    WritePrivateProfileStringW(L"lifesteal", L"bossVitalityAtLeast", num, ini);
    for (int i = 0; i < ActCount; ++i) {
        ClampRule(&g_life.rule[i]);
        const wchar_t* sec = Section(i);
        swprintf_s(num, L"%.4f", g_life.rule[i].vitalityPercent);
        WritePrivateProfileStringW(sec, L"vitalityPercent", num, ini);
        swprintf_s(num, L"%d", g_life.rule[i].vitalityEvery);
        WritePrivateProfileStringW(sec, L"vitalityEvery", num, ini);
        swprintf_s(num, L"%d", g_life.rule[i].emblems);
        WritePrivateProfileStringW(sec, L"emblems", num, ini);
        swprintf_s(num, L"%d", g_life.rule[i].emblemsEvery);
        WritePrivateProfileStringW(sec, L"emblemsEvery", num, ini);
        swprintf_s(num, L"%.4f", g_life.rule[i].posturePercent);
        WritePrivateProfileStringW(sec, L"posturePercent", num, ini);
        swprintf_s(num, L"%d", g_life.rule[i].postureEvery);
        WritePrivateProfileStringW(sec, L"postureEvery", num, ini);
        swprintf_s(num, L"%.4f", g_life.rule[i].gourdCharges);
        WritePrivateProfileStringW(sec, L"gourdCharges", num, ini);
        swprintf_s(num, L"%d", g_life.rule[i].gourdEvery);
        WritePrivateProfileStringW(sec, L"gourdEvery", num, ini);
    }
    g_life.blocksPerCharge = ClampInt(g_life.blocksPerCharge, 5, 10);
    g_life.gourdChargeCap = ClampInt(g_life.gourdChargeCap, 1, 10);
    PlayerSetGourdCap(g_life.gourdChargeCap);
}

const char* LifeWatchNote() { return g_watch; }
const char* LifeGuardNote() { return g_guard; }

bool LifeLanguageChosen() {
    return GetFileAttributesW(I18nPath()) != INVALID_FILE_ATTRIBUTES;
}

void LifeChooseLanguage(int lang) { I18nSet(lang); }

static bool Due(int action, int slot, int every) {
    if (every < 1) every = 1;
    g_every[action][slot] += 1;
    if (g_every[action][slot] < every) return false;
    g_every[action][slot] = 0;
    return true;
}

static int PercentOf(int maxValue, float pct) {
    if (pct <= 0.f || maxValue <= 0) return 0;
    int add = (int)((double)maxValue * (double)pct / 100.0);
    if (add < 1) add = 1;
    return add;
}

static const char* ActionName(int action) {
    switch (action) {
    case ActHit: return "hit";
    case ActBlock: return "block";
    case ActParry: return "parry";
    case ActDeath: return "deathblow";
    case ActDeathMini: return "mini-boss deathblow";
    case ActDeathBoss: return "boss deathblow";
    default: return "action";
    }
}

static void Apply(int action, int enemyMax) {
    if (action < 0 || action >= ActCount) return;
    if (!g_life.lifesteal && !g_life.gourd) return;
    Rule rule = g_life.rule[action];
    Vitals v{};
    if (!PlayerResolve(v)) return;
    int hp0 = v.hp;
    int go0 = v.gourd;
    if (g_life.lifesteal && rule.vitalityPercent > 0.f && Due(action, 0, rule.vitalityEvery))
        PlayerAddHp(PercentOf(v.maxHp, rule.vitalityPercent));
    if (g_life.lifesteal && rule.emblems > 0 && Due(action, 1, rule.emblemsEvery))
        PlayerAddEmblems(rule.emblems);
    if (g_life.lifesteal && rule.posturePercent > 0.f && Due(action, 2, rule.postureEvery))
        PlayerRestorePosture(rule.posturePercent);
    if (g_life.gourd && rule.gourdCharges > 0.f) {
        int every = rule.gourdEvery < 1 ? 1 : rule.gourdEvery;
        g_every[action][3] += 1;
        if (action == ActParry) g_life.gourdProgress = g_every[action][3] >= every ? 0 : g_every[action][3];
        if (g_every[action][3] >= every) {
            g_every[action][3] = 0;
            if (action == ActParry) g_life.gourdProgress = 0;
            g_gourdAcc[action] += rule.gourdCharges;
            if (g_gourdAcc[action] > 10.f) g_gourdAcc[action] = 10.f;
            int whole = (int)g_gourdAcc[action];
            if (whole >= 1 && PlayerAddGourd(whole, g_life.gourdChargeCap)) g_gourdAcc[action] -= (float)whole;
        }
    }
    g_paid[action] += 1;
    if (g_paid[action] == 1) {
        Vitals now{};
        PlayerResolve(now);
        Logf("Reward %s. Vitality %d -> %d, gourd %d -> %d. Enemy max %d.", ActionName(action), hp0, now.hp, go0, now.gourd, enemyMax);
    }
}

static int g_blockProgress = 0;

static void OnGourdGuard() {
    if (!g_life.gourd) return;
    int need = ClampInt(g_life.blocksPerCharge, 5, 10);
    g_blockProgress += 1;
    bool show = g_life.rule[ActParry].gourdCharges <= 0.f;
    if (show) g_life.gourdProgress = g_blockProgress;
    if (g_blockProgress < need) return;
    g_blockProgress = 0;
    if (show) g_life.gourdProgress = 0;
    if (!PlayerAddGourd(1, g_life.gourdChargeCap)) return;
    g_gourdPaid += 1;
    Vitals now{};
    if (PlayerResolve(now)) Logf("Gourd charge restored. Charges %d / %d.", now.gourd, g_life.gourdChargeCap);
}

static void Enqueue(int action, int maxHp) {
    if (!g_qInit) return;
    EnterCriticalSection(&g_qCs);
    int next = (g_qWrite + 1) % kQueueCap;
    if (next != g_qRead) {
        g_queue[g_qWrite].action = action;
        g_queue[g_qWrite].maxHp = maxHp;
        g_qWrite = next;
    }
    LeaveCriticalSection(&g_qCs);
}

static bool Readable(const void* p, size_t n) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    uintptr_t end = (uintptr_t)p + n;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return end <= regionEnd;
}

static bool IsWolfId(uint8_t* obj) {
    if (!Readable(obj + 0x80, 12)) return false;
    const wchar_t* id = (const wchar_t*)(obj + 0x80);
    return id[0] == L'c' && id[1] == L'0' && id[2] == L'0' && id[3] == L'0' && id[4] == L'0' && id[5] == 0;
}

static bool LooksLikeId(uint8_t* obj) {
    if (!Readable(obj + 0x80, 12)) return false;
    const wchar_t* id = (const wchar_t*)(obj + 0x80);
    if (id[0] != L'c' || id[5] != 0) return false;
    for (int i = 1; i <= 4; ++i) {
        if (id[i] < L'0' || id[i] > L'9') return false;
    }
    return true;
}

static void ObserveBody(uint8_t* obj, int newHp) {
    if (!obj || !Readable(obj + kCombatHp, 8)) return;
    if (IsWolfId(obj)) {
        PlayerNoteWolf(obj);
        return;
    }
    if (!LooksLikeId(obj)) {
        InterlockedIncrement(&g_idRejects);
        return;
    }
    int oldHp = *(int*)(obj + kCombatHp);
    int maxHp = *(int*)(obj + kCombatHp + 4);
    if (oldHp <= 0 || oldHp > 100000 || newHp < -10 || newHp > 100000) return;
    if (newHp >= oldHp) return;
    if (maxHp < oldHp || maxHp > 100000) return;
    DWORD now = GetTickCount();
    if (obj == g_lastObj && oldHp == g_lastOld && newHp == g_lastNew && now - g_lastTick < 5) return;
    g_lastObj = obj;
    g_lastOld = oldHp;
    g_lastNew = newHp;
    g_lastTick = now;
    if (!g_life.lifesteal) return;
    int action = ActHit;
    if (newHp <= 0) {
        if (maxHp >= g_life.bossVitalityAtLeast) action = ActDeathBoss;
        else if (maxHp >= g_life.miniBossVitalityAtLeast) action = ActDeathMini;
        else action = ActDeath;
    }
    Enqueue(action, maxHp);
}

extern "C" void HealthObserve(uint8_t* obj, int newHp) {
    __try {
        ObserveBody(obj, newHp);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static uint8_t* AllocNear(void* site, size_t n) {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    uintptr_t gran = info.dwAllocationGranularity ? info.dwAllocationGranularity : 0x10000;
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    uintptr_t image = exe ? (uintptr_t)exe : ((uintptr_t)site & ~(gran - 1));
    uintptr_t imageEnd = image;
    if (exe) {
        auto* dos = (IMAGE_DOS_HEADER*)exe;
        auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)exe + dos->e_lfanew);
        imageEnd = image + nt->OptionalHeader.SizeOfImage;
        imageEnd = (imageEnd + gran - 1) & ~(gran - 1);
    }
    for (int step = 0; step < 512; ++step) {
        uintptr_t cands[2] = {
            imageEnd + (uintptr_t)step * gran,
            image > (uintptr_t)(step + 1) * gran ? image - (uintptr_t)(step + 1) * gran : 0
        };
        for (int k = 0; k < 2; ++k) {
            if (cands[k] < 0x10000) continue;
            uint8_t* p = (uint8_t*)VirtualAlloc((void*)cands[k], n, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!p) continue;
            intptr_t rel = (intptr_t)p - ((intptr_t)site + 5);
            if (rel < (intptr_t)-2147483647 - 1 || rel > (intptr_t)2147483647) {
                VirtualFree(p, 0, MEM_RELEASE);
                continue;
            }
            return p;
        }
    }
    return nullptr;
}

static bool Relocatable(uint8_t* code) {
    hde64s hs{};
    unsigned len = hde64_disasm(code, &hs);
    return len == 6 && !(hs.flags & F_ERROR) && !(hs.flags & F_RELATIVE);
}

static uint8_t* FindPattern(uint8_t* start, size_t size, const uint8_t* sig, size_t sigLen) {
    if (size < sigLen) return nullptr;
    for (size_t i = 0; i + sigLen <= size; ++i) {
        if (memcmp(start + i, sig, sigLen) == 0) return start + i;
    }
    return nullptr;
}

static void PauseOthers(HANDLE* handles, int* n) {
    *n = 0;
    DWORD self = GetCurrentThreadId();
    DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
            if (*n >= 256) break;
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
            if (!thread) continue;
            if (SuspendThread(thread) == (DWORD)-1) {
                CloseHandle(thread);
                continue;
            }
            handles[(*n)++] = thread;
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

static void ResumeOthers(HANDLE* handles, int n) {
    for (int i = n - 1; i >= 0; --i) {
        ResumeThread(handles[i]);
        CloseHandle(handles[i]);
    }
}

static bool PatchSite(uint8_t* target, uint8_t* block) {
    if (!Relocatable(target) || !block) return false;
    uint8_t code[160];
    int n = 0;
    auto b = [&](uint8_t x) { code[n++] = x; };
    auto seq = [&](std::initializer_list<uint8_t> xs) { for (uint8_t x : xs) b(x); };
    seq({0x89, 0x83, 0x30, 0x01, 0x00, 0x00});
    seq({0x9C, 0x56, 0x50, 0x51, 0x52});
    seq({0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});
    seq({0x48, 0x89, 0xD9, 0x89, 0xC2, 0x48, 0x89, 0xE6, 0x48, 0x83, 0xE4, 0xF0, 0x48, 0x83, 0xEC, 0x20});
    b(0x48); b(0xB8);
    void* fn = (void*)&HealthObserve;
    memcpy(code + n, &fn, 8);
    n += 8;
    seq({0xFF, 0xD0, 0x48, 0x89, 0xF4});
    seq({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58});
    seq({0x5A, 0x59, 0x58, 0x5E, 0x9D});
    b(0x48); b(0xB8);
    void* ret = target + 6;
    memcpy(code + n, &ret, 8);
    n += 8;
    seq({0xFF, 0xE0});
    memcpy(block, code, n);
    DWORD old = 0;
    VirtualProtect(block, 256, PAGE_EXECUTE_READ, &old);
    if (!VirtualProtect(target, 6, PAGE_EXECUTE_READWRITE, &old)) return false;
    int32_t rel = (int32_t)((intptr_t)block - ((intptr_t)target + 5));
    target[0] = 0xE9;
    memcpy(target + 1, &rel, 4);
    target[5] = 0x90;
    VirtualProtect(target, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 6);
    FlushInstructionCache(GetCurrentProcess(), block, n);
    return true;
}

void LifeEnsureWatch() {
    if (g_watchOn) return;
    DWORD now = GetTickCount();
    static DWORD nextTry = 0;
    if (now < nextTry) return;
    nextTry = now + 2000;
    if (!g_qInit) {
        InitializeCriticalSection(&g_qCs);
        g_qInit = true;
    }
    if (!PlayerVersionOk()) {
        strcpy_s(g_watch, "wrong sekiro.exe");
        g_watchOn = true;
        return;
    }
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    auto* dos = (IMAGE_DOS_HEADER*)exe;
    auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)exe + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    const uint8_t sig[] = {0x8B, 0x00, 0x89, 0x83, 0x30, 0x01, 0x00, 0x00, 0x85, 0xC0};
    uint8_t* hits[4] = {};
    int seen = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections && seen < 4; ++i) {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint8_t* base = (uint8_t*)exe + sec[i].VirtualAddress;
        size_t size = sec[i].Misc.VirtualSize;
        uint8_t* cursor = base;
        size_t left = size;
        while (left > sizeof(sig) && seen < 4) {
            uint8_t* hit = FindPattern(cursor, left, sig, sizeof(sig));
            if (!hit) break;
            hits[seen++] = hit + 2;
            size_t step = (size_t)(hit - cursor) + sizeof(sig);
            if (step >= left) break;
            cursor = hit + sizeof(sig);
            left -= step;
        }
    }
    if (seen != 2 || !Relocatable(hits[0]) || !Relocatable(hits[1])) {
        const uint8_t hooked[] = {0x8B, 0x00, 0xE9};
        int already = 0;
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections && already < 4; ++i) {
            if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            uint8_t* base = (uint8_t*)exe + sec[i].VirtualAddress;
            size_t size = sec[i].Misc.VirtualSize;
            uint8_t* cursor = base;
            size_t left = size;
            while (left > sizeof(hooked) && already < 4) {
                uint8_t* hit = FindPattern(cursor, left, hooked, sizeof(hooked));
                if (!hit) break;
                already += 1;
                size_t step = (size_t)(hit - cursor) + sizeof(hooked);
                if (step >= left) break;
                cursor = hit + sizeof(hooked);
                left -= step;
            }
        }
        if (already > 0) {
            strcpy_s(g_watch, "old hook already there");
            Log("Health watch left alone. The old mod already patched those sites.");
            g_watchOn = true;
            return;
        }
        sprintf_s(g_watch, "pattern hits %d, not hooked", seen);
        static bool logged = false;
        if (!logged) {
            logged = true;
            Logf("Health watch left off. Pattern hits %d. Expected 2.", seen);
        }
        return;
    }
    uint8_t saved0[6];
    memcpy(saved0, hits[0], 6);
    uint8_t* block0 = AllocNear(hits[0], 256);
    uint8_t* block1 = AllocNear(hits[1], 256);
    if (!block0 || !block1) {
        if (block0) VirtualFree(block0, 0, MEM_RELEASE);
        if (block1) VirtualFree(block1, 0, MEM_RELEASE);
        strcpy_s(g_watch, "no nearby code page");
        LogFail("Health watch could not allocate a nearby code page.");
        g_watchOn = true;
        return;
    }
    HANDLE paused[256];
    int pausedN = 0;
    PauseOthers(paused, &pausedN);
    bool first = PatchSite(hits[0], block0);
    bool ok = first && PatchSite(hits[1], block1);
    if (!ok && first) {
        DWORD old = 0;
        if (VirtualProtect(hits[0], 6, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy(hits[0], saved0, 6);
            VirtualProtect(hits[0], 6, old, &old);
            FlushInstructionCache(GetCurrentProcess(), hits[0], 6);
        }
    }
    ResumeOthers(paused, pausedN);
    g_watchOn = true;
    if (!ok) {
        strcpy_s(g_watch, "patch failed");
        LogFail("Health watch patch failed.");
        return;
    }
    strcpy_s(g_watch, "pattern hits 2, hooked 2");
    Log("Health watch on. Pattern hits 2, hooked 2.");
}

static void Drain() {
    if (!g_qInit) return;
    Pending local[16];
    int n = 0;
    EnterCriticalSection(&g_qCs);
    while (g_qRead != g_qWrite && n < 16) {
        local[n++] = g_queue[g_qRead];
        g_qRead = (g_qRead + 1) % kQueueCap;
    }
    LeaveCriticalSection(&g_qCs);
    for (int i = 0; i < n; ++i) {
        Apply(local[i].action, local[i].maxHp);
        if (local[i].action == ActBlock && g_life.rule[ActBlock].gourdCharges <= 0.f) OnGourdGuard();
    }
    if (!g_idLogged && g_idRejects > 0) {
        g_idLogged = true;
        Log("A health write had no character id. Those writes are ignored.");
    }
}

void LifeTick() {
    if (!g_loaded) LifeLoad();
    Drain();
    Vitals v{};
    if (!PlayerResolve(v)) return;
    if (!g_statsLogged) {
        g_statsLogged = true;
        Logf("Wolf vitality %d / %d, posture %d / %d, gourd %d.", v.hp, v.maxHp, v.posture, v.maxPosture, v.gourd);
    }
    if (g_life.infiniteEmblems) PlayerFillEmblems();
    if (g_life.infiniteHp) PlayerFillHp();
    else if (g_life.undeath) {
        PlayerFloorHp();
        PlayerSetNoDead(true);
    } else {
        PlayerSetNoDead(false);
    }
}

static bool WolfFromCtrl(uint8_t* ctrl) {
    if (!Readable(ctrl, 0x10)) return false;
    uint8_t* chr = *(uint8_t**)(ctrl + 8);
    if (!Readable(chr + 0x1FF8, 8)) return false;
    uint8_t* box = *(uint8_t**)(chr + 0x1FF8);
    if (!Readable(box + 0x18, 8)) return false;
    uint8_t* combat = *(uint8_t**)(box + 0x18);
    if (IsWolfId(combat)) return true;
    if (!LooksLikeId(combat)) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            Log("A deflect had no character id. It was ignored.");
        }
    }
    return false;
}

static bool FreshHit(uint8_t** slot, DWORD* tick, uint8_t* dmg) {
    DWORD now = GetTickCount();
    if (dmg == *slot && now - *tick < 20) return false;
    *slot = dmg;
    *tick = now;
    return true;
}

extern "C" void ParryObserve(uint8_t* ctrl, uint8_t* dmg) {
    __try {
        static uint8_t* slot = nullptr;
        static DWORD tick = 0;
        if (!dmg || !FreshHit(&slot, &tick, dmg)) return;
        if (!WolfFromCtrl(ctrl)) return;
        if (!g_life.lifesteal && !g_life.gourd) return;
        Enqueue(ActParry, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

extern "C" void BlockObserve(uint8_t* ctrl, uint8_t* dmg) {
    __try {
        static uint8_t* slot = nullptr;
        static DWORD tick = 0;
        if (!dmg || !Readable(dmg + 0x1C7, 1)) return;
        if (dmg[0x1C7] != 0) return;
        if (!FreshHit(&slot, &tick, dmg)) return;
        if (!WolfFromCtrl(ctrl)) return;
        if (!g_life.lifesteal && !g_life.gourd) return;
        Enqueue(ActBlock, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static uint8_t* FindOne(const uint8_t* sig, size_t sigLen, int* seen) {
    *seen = 0;
    uint8_t* only = nullptr;
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    auto* dos = (IMAGE_DOS_HEADER*)exe;
    auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)exe + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections && *seen < 3; ++i) {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint8_t* base = (uint8_t*)exe + sec[i].VirtualAddress;
        size_t left = sec[i].Misc.VirtualSize;
        uint8_t* cursor = base;
        while (left >= sigLen && *seen < 3) {
            uint8_t* hit = FindPattern(cursor, left, sig, sigLen);
            if (!hit) break;
            *seen += 1;
            only = hit;
            size_t step = (size_t)(hit - cursor) + sigLen;
            if (step >= left) break;
            cursor = hit + sigLen;
            left -= step;
        }
    }
    return *seen == 1 ? only : nullptr;
}

static bool PatchGuard(uint8_t* target, uint8_t* block, void* fn) {
    if (!target || !block || !fn) return false;
    uint8_t code[160];
    int n = 0;
    auto b = [&](uint8_t x) { code[n++] = x; };
    auto seq = [&](std::initializer_list<uint8_t> xs) { for (uint8_t x : xs) b(x); };
    memcpy(code, target, 8);
    n = 8;
    seq({0x9C, 0x56, 0x50, 0x51, 0x52});
    seq({0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});
    seq({0x49, 0x8B, 0xCC, 0x4C, 0x89, 0xFA});
    seq({0x48, 0x89, 0xE6, 0x48, 0x83, 0xE4, 0xF0, 0x48, 0x83, 0xEC, 0x20});
    b(0x48); b(0xB8);
    memcpy(code + n, &fn, 8);
    n += 8;
    seq({0xFF, 0xD0, 0x48, 0x89, 0xF4});
    seq({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58});
    seq({0x5A, 0x59, 0x58, 0x5E, 0x9D});
    b(0x48); b(0xB8);
    void* ret = target + 8;
    memcpy(code + n, &ret, 8);
    n += 8;
    seq({0xFF, 0xE0});
    memcpy(block, code, n);
    DWORD old = 0;
    VirtualProtect(block, 256, PAGE_EXECUTE_READ, &old);
    if (!VirtualProtect(target, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    int32_t rel = (int32_t)((intptr_t)block - ((intptr_t)target + 5));
    target[0] = 0xE9;
    memcpy(target + 1, &rel, 4);
    target[5] = 0x90;
    target[6] = 0x90;
    target[7] = 0x90;
    VirtualProtect(target, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 8);
    FlushInstructionCache(GetCurrentProcess(), block, n);
    return true;
}

void LifeEnsureGuard() {
    if (g_guardOn) return;
    DWORD now = GetTickCount();
    static DWORD nextTry = 0;
    if (now < nextTry) return;
    nextTry = now + 2000;
    if (!g_qInit) {
        InitializeCriticalSection(&g_qCs);
        g_qInit = true;
    }
    if (!PlayerVersionOk()) {
        strcpy_s(g_guard, "wrong sekiro.exe");
        g_guardOn = true;
        return;
    }
    // The damage function sets [info+1C7] to 1 only for a just-guard, then
    // sets [info+164] to 1 for a held block. A deflect also falls through to
    // the block store, so the block callback ignores a contact that is already a deflect.
    const uint8_t parrySig[] = {
        0x41, 0xC6, 0x87, 0xC7, 0x01, 0x00, 0x00, 0x01,
        0x41, 0x80, 0x8F, 0x31, 0x02, 0x00, 0x00, 0x40
    };
    const uint8_t blockSig[] = {
        0x41, 0xC6, 0x87, 0x64, 0x01, 0x00, 0x00, 0x01,
        0x41, 0x0F, 0xB6, 0xBF, 0x64, 0x01, 0x00, 0x00
    };
    int parrySeen = 0;
    int blockSeen = 0;
    uint8_t* parry = FindOne(parrySig, sizeof(parrySig), &parrySeen);
    uint8_t* block = FindOne(blockSig, sizeof(blockSig), &blockSeen);
    if (!parry || !block) {
        sprintf_s(g_guard, "pattern hits %d / %d", parrySeen, blockSeen);
        static bool logged = false;
        if (!logged) {
            logged = true;
            Logf("Guard hook left off. Deflect sites %d, block sites %d. Expected 1 and 1.", parrySeen, blockSeen);
        }
        return;
    }
    uint8_t saved[8];
    memcpy(saved, parry, 8);
    uint8_t* page0 = AllocNear(parry, 256);
    uint8_t* page1 = AllocNear(block, 256);
    if (!page0 || !page1) {
        if (page0) VirtualFree(page0, 0, MEM_RELEASE);
        if (page1) VirtualFree(page1, 0, MEM_RELEASE);
        strcpy_s(g_guard, "no nearby code page");
        LogFail("Guard hook could not allocate a nearby code page.");
        g_guardOn = true;
        return;
    }
    HANDLE paused[256];
    int pausedN = 0;
    PauseOthers(paused, &pausedN);
    bool first = PatchGuard(parry, page0, (void*)&ParryObserve);
    bool ok = first && PatchGuard(block, page1, (void*)&BlockObserve);
    if (!ok && first) {
        DWORD old = 0;
        if (VirtualProtect(parry, 8, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy(parry, saved, 8);
            VirtualProtect(parry, 8, old, &old);
            FlushInstructionCache(GetCurrentProcess(), parry, 8);
        }
    }
    ResumeOthers(paused, pausedN);
    g_guardOn = true;
    if (!ok) {
        strcpy_s(g_guard, "patch failed");
        LogFail("Guard hook patch failed.");
        return;
    }
    strcpy_s(g_guard, "deflect 1, block 1");
    Log("Guard hook on. Deflect site 1, block site 1.");
}
