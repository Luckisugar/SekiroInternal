#include "player.h"
#include "config.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

static uint8_t* g_wolf = nullptr;
static int* g_emblemQty = nullptr;
static int* g_gourdQty = nullptr;
static int g_gourdCap = 10;
static bool g_versionChecked = false;
static bool g_versionOk = false;
static bool g_noDeadOwned = false;
static bool g_noDeadBad = false;
static bool g_gourdMiss = false;


static bool Readable(const void* p, size_t n) {
    if (!p || n == 0) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    uintptr_t end = (uintptr_t)p + n;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return end <= regionEnd;
}

static bool WolfId(uint8_t* obj) {
    if (!obj || !Readable(obj + 0x80, 12)) return false;
    const wchar_t* id = (const wchar_t*)(obj + 0x80);
    return id[0] == L'c' && id[1] == L'0' && id[2] == L'0' && id[3] == L'0' && id[4] == L'0' && id[5] == 0;
}

bool PlayerVersionOk() {
    if (g_versionChecked) return g_versionOk;
    g_versionChecked = true;
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    if (!exe) {
        LogFail("sekiro.exe is not loaded.");
        return false;
    }
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(exe, path, MAX_PATH)) {
        LogFail("sekiro.exe path is not readable.");
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info)) {
        LogFail("sekiro.exe size could not be read.");
        return false;
    }
    unsigned size = info.nFileSizeLow;
    g_versionOk = size == kSekiroExeBytes && info.nFileSizeHigh == 0;
    if (!g_versionOk) Logf("sekiro.exe is %u bytes. This DLL knows %u. Writes are off.", size, kSekiroExeBytes);
    else Log("sekiro.exe is patch 1.06.");
    return g_versionOk;
}

static uint8_t* Player() {
    if (!PlayerVersionOk()) return nullptr;
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    uint8_t* slot = (uint8_t*)exe + kGameDataManRva;
    if (!Readable(slot, sizeof(void*))) return nullptr;
    uint8_t* man = *(uint8_t**)slot;
    if (!Readable(man, 0x20)) return nullptr;
    uint8_t* player = *(uint8_t**)(man + 0x08);
    if (!Readable(player, 0x200)) return nullptr;
    return player;
}

void PlayerNoteWolf(uint8_t* obj) {
    if (WolfId(obj)) g_wolf = obj;
}

bool PlayerCombatSeen() {
    return g_wolf && WolfId(g_wolf);
}

static int* ScanList(uint8_t* head, int cap) {
    if (cap < 1 || cap > 2048 || !head) return nullptr;
    size_t bytes = (size_t)cap * 0x10;
    if (!Readable(head, bytes)) return nullptr;
    for (int i = 0; i < cap; ++i) {
        uint8_t* e = head + (size_t)i * 0x10;
        unsigned id = *(unsigned*)(e + 4);
        int qty = *(int*)(e + 8);
        if (id == kGourdItem && qty >= 0 && qty <= 10) return (int*)(e + 8);
        id = *(unsigned*)e;
        qty = *(int*)(e + 4);
        if (id == kGourdItem && qty >= 0 && qty <= 10) return (int*)(e + 4);
    }
    return nullptr;
}

static int* FindInList(uint8_t* player, int capOff, int headOff) {
    if (!Readable(player + capOff, 8)) return nullptr;
    int cap = *(int*)(player + capOff);
    uint8_t* head = *(uint8_t**)(player + headOff);
    return ScanList(head, cap);
}

static bool FindGourd(uint8_t* player) {
    if (g_gourdQty) return true;
    static DWORD nextTry = 0;
    DWORD now = GetTickCount();
    if (now < nextTry) return false;
    nextTry = now + 2000;
    int* qty = FindInList(player, kNormalItemsCap, kNormalItems);
    if (!qty) qty = FindInList(player, kKeyItemsCap, kKeyItems);
    if (!qty) return false;
    g_gourdQty = qty;
    Logf("Healing Gourd stack found. Quantity %d.", *qty);
    return true;
}

void PlayerSetGourdCap(int cap) {
    if (cap < 1) cap = 1;
    if (cap > 10) cap = 10;
    g_gourdCap = cap;
}

bool PlayerResolve(Vitals& out) {
    memset(&out, 0, sizeof(out));
    out.gourdCap = g_gourdCap;
    uint8_t* player = Player();
    if (!player) return false;
    if (!Readable(player + kHp, 16) || !Readable(player + kPosture, 8)) return false;
    out.hp = *(int*)(player + kHp);
    out.maxHp = *(int*)(player + kMaxHp);
    out.posture = *(int*)(player + kPosture);
    out.maxPosture = *(int*)(player + kMaxPosture);
    if (out.maxHp < 1 || out.maxHp > 100000 || out.hp < 0 || out.hp > out.maxHp + 50) return false;
    if (out.maxPosture < 1 || out.maxPosture > 100000 || out.posture < 0 || out.posture > out.maxPosture + 50) return false;
    out.valid = true;
    out.combat = PlayerCombatSeen();
    if (Readable(player + kMaxEmblems, 6)) {
        int maxE = *(unsigned short*)(player + kMaxEmblems);
        if (maxE >= 1 && maxE <= 40) out.maxEmblems = maxE;
        int cur = *(int*)(player + kMaxEmblems + 2);
        if (!g_emblemQty && out.maxEmblems > 0 && cur >= 0 && cur <= out.maxEmblems) {
            g_emblemQty = (int*)(player + kMaxEmblems + 2);
            Logf("Emblem candidate %d / %d. Compare it with the HUD.", cur, out.maxEmblems);
        }
    }
    if (g_emblemQty && Readable(g_emblemQty, 4)) out.emblems = *g_emblemQty;
    if (!g_gourdQty) {
        if (!FindGourd(player) && !g_gourdMiss) {
            g_gourdMiss = true;
            Log("Healing Gourd stack not found yet.");
        }
    }
    if (g_gourdQty && Readable(g_gourdQty, 4)) out.gourd = *g_gourdQty;
    return true;
}

template <typename Fn>
static void WriteCombatHp(Fn pick) {
    if (!g_wolf || !WolfId(g_wolf)) {
        g_wolf = nullptr;
        return;
    }
    if (!Readable(g_wolf + kCombatHp, 8)) return;
    int cur = *(int*)(g_wolf + kCombatHp);
    int mx = *(int*)(g_wolf + kCombatHp + 4);
    if (mx < 1 || mx > 100000 || cur < -10 || cur > mx + 50) return;
    int next = pick(cur, mx);
    if (next < 0) next = 0;
    if (next > mx) next = mx;
    if (next != cur) *(int*)(g_wolf + kCombatHp) = next;
}

template <typename Fn>
static void WriteStatHp(Fn pick) {
    uint8_t* player = Player();
    if (!player || !Readable(player + kHp, 8)) return;
    int cur = *(int*)(player + kHp);
    int mx = *(int*)(player + kMaxHp);
    if (mx < 1 || mx > 100000 || cur < -10 || cur > mx + 50) return;
    int next = pick(cur, mx);
    if (next < 0) next = 0;
    if (next > mx) next = mx;
    if (next != cur) *(int*)(player + kHp) = next;
    WriteCombatHp(pick);
}

void PlayerAddHp(int add) {
    if (add <= 0 || !PlayerVersionOk()) return;
    WriteStatHp([add](int cur, int mx) {
        int next = cur + add;
        return next > mx ? mx : next;
    });
}

void PlayerFillHp() {
    if (!PlayerVersionOk()) return;
    WriteStatHp([](int, int mx) { return mx; });
}

void PlayerFloorHp() {
    if (!PlayerVersionOk()) return;
    WriteStatHp([](int cur, int) { return cur < 1 ? 1 : cur; });
}

void PlayerFillEmblems() {
    if (!PlayerVersionOk() || !g_emblemQty || !Readable(g_emblemQty, 4)) return;
    uint8_t* player = Player();
    if (!player || !Readable(player + kMaxEmblems, 2)) return;
    int cap = *(unsigned short*)(player + kMaxEmblems);
    int cur = *g_emblemQty;
    if (cap < 1 || cap > 40 || cur < 0 || cur > cap) return;
    if (cur != cap) *g_emblemQty = cap;
}

void PlayerAddEmblems(int count) {
    if (count <= 0 || !PlayerVersionOk() || !g_emblemQty || !Readable(g_emblemQty, 4)) return;
    uint8_t* player = Player();
    if (!player || !Readable(player + kMaxEmblems, 2)) return;
    int cap = *(unsigned short*)(player + kMaxEmblems);
    int cur = *g_emblemQty;
    if (cap < 1 || cap > 40 || cur < 0 || cur > cap) return;
    int next = cur + count;
    if (next > cap) next = cap;
    *g_emblemQty = next;
}

void PlayerRestorePosture(float pct) {
    if (pct <= 0.f || !PlayerVersionOk()) return;
    uint8_t* player = Player();
    if (!player || !Readable(player + kPosture, 8)) return;
    int cur = *(int*)(player + kPosture);
    int mx = *(int*)(player + kMaxPosture);
    if (mx < 1 || mx > 100000 || cur < 0 || cur > mx + 50) return;
    int sub = (int)((double)mx * (double)pct / 100.0);
    if (sub < 1) sub = 1;
    int next = cur - sub;
    if (next < 0) next = 0;
    *(int*)(player + kPosture) = next;
}

static volatile LONG g_gourdPending = 0;

bool PlayerAddGourd(int charges, int cap) {
    if (charges <= 0) return false;
    if (cap < 1) cap = g_gourdCap;
    if (cap > 10) cap = 10;
    g_gourdCap = cap;
    if (g_gourdQty && Readable(g_gourdQty, 4)) {
        int cur = *g_gourdQty;
        if (cur < 0 || cur > cap) return false;
        int next = cur + charges;
        if (next > cap) next = cap;
        if (next == cur) return false;
        *g_gourdQty = next;
        return true;
    }
    InterlockedAdd(&g_gourdPending, charges);
    return true;
}

void PlayerFlushGourd() {
    LONG n = InterlockedExchange(&g_gourdPending, 0);
    if (n <= 0) return;
    if (n > 10) n = 10;
    if (!PlayerVersionOk()) return;
    uint8_t* player = Player();
    if (!player || !Readable(player + 0x278, 0x20)) {
        InterlockedAdd(&g_gourdPending, n);
        return;
    }
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    uint8_t* fn = (uint8_t*)exe + 0x7A2BD0;
    if (fn[0] != 0x83 || fn[1] != 0xFA || fn[2] != 0x1C) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            Log("Gourd add left alone. The game function does not match patch 1.06.");
        }
        InterlockedAdd(&g_gourdPending, n);
        return;
    }
    uint8_t* equip = player + 0x278;
    uint32_t* spec = (uint32_t*)(equip + 0x10);
    spec[0] = 0x40000000;
    spec[1] = 0x00000BB8;
    spec[2] = (uint32_t)n;
    spec[3] = 1;
    using AddFn = void(__fastcall*)(void*, int);
    ((AddFn)fn)(equip, 2);
    Logf("Gourd add requested. Charges %ld.", n);
}

void PlayerSetNoDead(bool on) {
    if (!PlayerVersionOk()) return;
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    if (!exe) return;
    uint8_t* flag = (uint8_t*)exe + kDebugFlagsRva + kPlayerNoDeadDelta;
    if (!Readable(flag, 1)) return;
    uint8_t v = *flag;
    if (v > 1 && !g_noDeadOwned) {
        if (!g_noDeadBad) {
            g_noDeadBad = true;
            Logf("No-death flag byte is %u, not 0 or 1. Left alone.", (unsigned)v);
        }
        return;
    }
    if (on) {
        if ((v & 1) == 0) {
            *flag = (uint8_t)(v | 1);
            g_noDeadOwned = true;
            Log("Undeath flag set.");
        }
    } else if (g_noDeadOwned) {
        *flag = (uint8_t)(*flag & ~1u);
        g_noDeadOwned = false;
        Log("Undeath flag cleared.");
    }
}
