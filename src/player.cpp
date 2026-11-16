#include "player.h"
#include "config.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

static uint8_t* g_wolf = nullptr;
static volatile LONG g_hpFloor = 0;
static volatile ULONGLONG g_hpHoldUntil = 0;
static ULONGLONG g_hpStableSince = 0;
static int* g_gourdSlots[16] = {};
static int g_gourdSlotN = 0;
static int g_gourdFloor = 0;
static ULONGLONG g_gourdHoldUntil = 0;
static ULONGLONG g_gourdStableSince = 0;
// The game stores the old vitality and gourd count again right after a reward.
// Raise the reward back until it has sat still. Never write a lower number
// over a heal, and stop once it sticks so a drink or a hit can change it.
static const ULONGLONG kRewardHoldMs = 800;
static const ULONGLONG kRewardStableMs = 160;
static int* g_gourdQty = nullptr;
static int g_emblemFloor = 0;
static ULONGLONG g_emblemHoldUntil = 0;
static ULONGLONG g_emblemStableSince = 0;
static bool g_emblemMiss = false;
static bool g_emblemLogged = false;
static int g_gourdCap = 10;
static bool g_versionChecked = false;
static bool g_versionOk = false;
static bool g_noDeadOwned = false;
static bool g_noDeadBad = false;
static bool g_gourdMiss = false;
static bool g_gourdNow = false;

static int EmblemCap(uint8_t* player);
static int EmblemCarry(uint8_t* player, int cap);


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
    if (!WolfId(obj)) return;
    if (g_wolf == obj) return;
    g_wolf = obj;
    int cur = 0;
    int mx = 0;
    if (Readable(obj + kCombatHp, 8)) {
        cur = *(int*)(obj + kCombatHp);
        mx = *(int*)(obj + kCombatHp + 4);
    }
    Logf("Wolf combat health latched. Bar %d / %d.", cur, mx);
}

void PlayerKeepWolf(uint8_t* obj) {
    if (!g_wolf) PlayerNoteWolf(obj);
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
        if (id == kGourdItem && qty >= 0 && qty <= 20) return (int*)(e + 8);
        id = *(unsigned*)e;
        qty = *(int*)(e + 4);
        if (id == kGourdItem && qty >= 0 && qty <= 20) return (int*)(e + 4);
    }
    return nullptr;
}

static int* FindInList(uint8_t* player, int capOff, int headOff) {
    if (!Readable(player + capOff, 8)) return nullptr;
    int cap = *(int*)(player + capOff);
    uint8_t* head = *(uint8_t**)(player + headOff);
    return ScanList(head, cap);
}

static bool GourdIdStill(int* qty) {
    if (!qty || !Readable((uint8_t*)qty - 4, 8)) return false;
    return *(unsigned*)((uint8_t*)qty - 4) == kGourdItem;
}

static bool FindGourd(uint8_t* player) {
    if (g_gourdQty && GourdIdStill(g_gourdQty)) return true;
    g_gourdQty = nullptr;
    static DWORD nextTry = 0;
    DWORD now = GetTickCount();
    if (!g_gourdNow && now < nextTry) return false;
    g_gourdNow = false;
    nextTry = now + 2000;
    int* qty = FindInList(player, kNormalItemsCap, kNormalItems);
    if (!qty) qty = FindInList(player, kKeyItemsCap, kKeyItems);
    if (!qty) return false;
    g_gourdQty = qty;
    Logf("Healing Gourd stack found. Quantity %d.", *qty);
    return true;
}

void PlayerSetGourdCap(int cap) {
    if (cap < 0) cap = 0;
    if (cap > 20) cap = 20;
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
    int emblemCap = EmblemCap(player);
    out.maxEmblems = emblemCap;
    int carry = EmblemCarry(player, emblemCap);
    if (carry >= 0) out.emblems = carry;
    else if (emblemCap > 0 && Readable(player + kEmblemHud, 8)) {
        uint8_t* hud = *(uint8_t**)(player + kEmblemHud);
        if (hud && Readable(hud + 0xC, 2)) {
            int cur = *(unsigned short*)(hud + 0xC);
            if (cur >= 0 && cur <= emblemCap + 15) out.emblems = cur;
        }
    }
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
        static bool logged = false;
        if (!logged) {
            logged = true;
            Log("Vitality bar was not written. Wolf's combat body is not latched yet.");
        }
        return;
    }
    if (!Readable(g_wolf + kCombatHp, 8)) return;
    int cur = *(int*)(g_wolf + kCombatHp);
    int mx = *(int*)(g_wolf + kCombatHp + 4);
    if (mx < 1 || mx > 100000 || cur < -10 || cur > mx + 50) return;
    int next = pick(cur, mx);
    if (next < 0) next = 0;
    if (next > mx) next = mx;
    if (next == cur) return;
    *(int*)(g_wolf + kCombatHp) = next;
    static bool logged = false;
    if (!logged) {
        logged = true;
        Logf("Vitality bar write %d -> %d.", cur, next);
    }
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

static void WriteHpField(uint8_t* addr, int next, int maxAddrDelta) {
    if (!addr || !Readable(addr, 8)) return;
    int cur = *(int*)addr;
    int mx = *(int*)(addr + maxAddrDelta);
    if (mx < 1 || mx > 100000 || cur < -10 || cur > mx + 50) return;
    if (next < 0) next = 0;
    if (next > mx) next = mx;
    if (next != cur) *(int*)addr = next;
}

static void WriteHpValue(int next) {
    uint8_t* player = Player();
    if (player) WriteHpField(player + kHp, next, 4);
    if (g_wolf && WolfId(g_wolf)) WriteHpField(g_wolf + kCombatHp, next, 4);
}

static bool ReadHpField(uint8_t* addr, int* cur, int* mx) {
    if (!addr || !Readable(addr, 8)) return false;
    int c = *(int*)addr;
    int m = *(int*)(addr + 4);
    if (m < 1 || m > 100000 || c < -10 || c > m + 50) return false;
    *cur = c;
    *mx = m;
    return true;
}

static void RaiseHpField(uint8_t* addr, int floor) {
    int cur = 0;
    int mx = 0;
    if (!ReadHpField(addr, &cur, &mx)) return;
    if (cur >= floor) return;
    int next = floor;
    if (next > mx) next = mx;
    if (next < 0) next = 0;
    if (next != cur) *(int*)addr = next;
}

static void RaiseHpFloor(int floor) {
    uint8_t* player = Player();
    if (player) RaiseHpField(player + kHp, floor);
    if (g_wolf && WolfId(g_wolf)) RaiseHpField(g_wolf + kCombatHp, floor);
}

static bool HpAtFloor(int floor) {
    int seen = 0;
    int below = 0;
    auto check = [&](uint8_t* addr) {
        int cur = 0;
        int mx = 0;
        if (!ReadHpField(addr, &cur, &mx)) return;
        seen += 1;
        if (cur < floor) below += 1;
    };
    uint8_t* player = Player();
    if (player) check(player + kHp);
    if (g_wolf && WolfId(g_wolf)) check(g_wolf + kCombatHp);
    return seen > 0 && below == 0;
}

static void ClearHpHold() {
    g_hpHoldUntil = 0;
    g_hpStableSince = 0;
}

void PlayerAddHp(int add) {
    if (add <= 0 || !PlayerVersionOk()) return;
    int cur = -1;
    int mx = 0;
    if (g_wolf && WolfId(g_wolf) && Readable(g_wolf + kCombatHp, 8)) {
        cur = *(int*)(g_wolf + kCombatHp);
        mx = *(int*)(g_wolf + kCombatHp + 4);
    }
    uint8_t* player = Player();
    if (player && Readable(player + kHp, 8)) {
        int stat = *(int*)(player + kHp);
        int statMx = *(int*)(player + kMaxHp);
        if (cur < 0 || (stat >= 0 && stat < cur)) {
            cur = stat;
            mx = statMx;
        }
    }
    if (mx < 1 || mx > 100000 || cur < 0) return;
    int next = cur + add;
    if (next > mx) next = mx;
    if (next < 0) next = 0;
    if (next == cur) return;
    g_hpFloor = next;
    g_hpStableSince = 0;
    g_hpHoldUntil = GetTickCount64() + kRewardHoldMs;
    WriteHpValue(next);
    Logf("Vitality bar write %d -> %d.", cur, next);
}

void PlayerEnforceHp() {
    ULONGLONG until = g_hpHoldUntil;
    if (until == 0) return;
    ULONGLONG now = GetTickCount64();
    if (now >= until) {
        ClearHpHold();
        return;
    }
    int floor = (int)g_hpFloor;
    if (HpAtFloor(floor)) {
        if (g_hpStableSince == 0) g_hpStableSince = now;
        else if (now - g_hpStableSince >= kRewardStableMs) ClearHpHold();
        return;
    }
    g_hpStableSince = 0;
    RaiseHpFloor(floor);
}

extern "C" int PlayerApplyHeal(uint8_t* body, int loaded, uint8_t* source) {
    if (!body || !WolfId(body)) return loaded;
    g_wolf = body;
    ULONGLONG until = g_hpHoldUntil;
    if (until == 0 || GetTickCount64() >= until || !source) return loaded;
    // Only the real vitality fields. The same load also feeds the red damage
    // streak; writing full vitality there stretches that streak past the bar
    // and the HUD keeps growing it until the game stalls.
    uint8_t* player = Player();
    const bool combat = source == body + kCombatHp;
    const bool stat = player && source == player + kHp;
    if (!combat && !stat) return loaded;
    int floor = (int)g_hpFloor;
    if (loaded >= floor) return loaded;
    int mx = 0;
    if (Readable(body + kCombatHp + 4, 4)) mx = *(int*)(body + kCombatHp + 4);
    if ((mx < 1 || mx > 100000) && player && Readable(player + kMaxHp, 4)) mx = *(int*)(player + kMaxHp);
    if (mx < 1 || mx > 100000) return loaded;
    int out = floor;
    if (out > mx) out = mx;
    if (out < 0) out = 0;
    return out;
}

void PlayerFillHp() {
    if (!PlayerVersionOk()) return;
    WriteStatHp([](int, int mx) { return mx; });
}

void PlayerFloorHp() {
    if (!PlayerVersionOk()) return;
    WriteStatHp([](int cur, int) { return cur < 1 ? 1 : cur; });
}

static int EmblemCap(uint8_t* player) {
    if (!player || !Readable(player + kMaxEmblems, 2)) return 0;
    int cap = *(unsigned short*)(player + kMaxEmblems);
    if (cap < 1 || cap > 40) return 0;
    return cap;
}

static void RememberEmblem(int* qty, int cap, int** slots, int* n) {
    if (!qty || !Readable(qty, 4) || *n >= 8) return;
    int cur = *qty;
    if (cur < 0 || cur > cap + 15) return;
    for (int i = 0; i < *n; ++i) if (slots[i] == qty) return;
    slots[(*n)++] = qty;
}

static void ScanGoods(uint8_t* head, int count, int cap, int** slots, int* n) {
    if (count < 1 || count > 2048 || !head || !Readable(head, (size_t)count * 0x10)) return;
    for (int i = 0; i < count; ++i) {
        uint8_t* e = head + (size_t)i * 0x10;
        if (*(unsigned*)(e + 4) == kEmblemItem) RememberEmblem((int*)(e + 8), cap, slots, n);
        else if (*(unsigned*)e == kEmblemItem) RememberEmblem((int*)(e + 4), cap, slots, n);
    }
}

static int FindEmblemSlots(uint8_t* player, int cap, int** slots) {
    int n = 0;
    if (!player) return 0;
    // Same goods lists as the Healing Gourd. The old prosthetic walk at +0x858
    // never contains this item, which is why every reward logged emblems 0 -> 0.
    if (Readable(player + kNormalItemsCap, 8))
        ScanGoods(*(uint8_t**)(player + kNormalItems), *(int*)(player + kNormalItemsCap), cap, slots, &n);
    if (Readable(player + kKeyItemsCap, 8))
        ScanGoods(*(uint8_t**)(player + kKeyItems), *(int*)(player + kKeyItemsCap), cap, slots, &n);
    if (!Readable(player + kEmblemInv, 8)) return n;
    uint8_t* inv = *(uint8_t**)(player + kEmblemInv);
    if (!Readable(inv, 0x70)) return n;
    int total = *(int*)(inv + 0x10);
    int split = *(int*)(inv + 0x24);
    if (total < 0 || total > 512 || split < 0 || split > total) return n;
    ScanGoods(*(uint8_t**)(inv + 0x58), split, cap, slots, &n);
    ScanGoods(*(uint8_t**)(inv + 0x48), total - split, cap, slots, &n);
    return n;
}

static void WriteEmblemHud(uint8_t* player, int qty, int cap) {
    if (!Readable(player + kEmblemHud, 8)) return;
    uint8_t* hud = *(uint8_t**)(player + kEmblemHud);
    if (!hud || !Readable(hud + 0xC, 2)) return;
    int cur = *(unsigned short*)(hud + 0xC);
    if (cur < 0 || cur > cap + 15) return;
    if (qty < 0) qty = 0;
    if (qty > cap + 15) qty = cap + 15;
    if (cur != qty) *(unsigned short*)(hud + 0xC) = (unsigned short)qty;
}

static void WriteEmblemFloor() {
    uint8_t* player = Player();
    if (!player) return;
    int cap = EmblemCap(player);
    if (cap < 1) return;
    int floor = g_emblemFloor;
    if (floor < 0) floor = 0;
    if (floor > cap + 15) floor = cap + 15;
    int* slots[8];
    int n = FindEmblemSlots(player, cap, slots);
    int* pick = nullptr;
    int before = -1;
    int* extra = nullptr;
    int extraV = 0;
    for (int i = 0; i < n; ++i) {
        int* q = slots[i];
        if (!Readable(q, 4)) continue;
        int cur = *q;
        if (cur < 0 || cur > cap + 15) continue;
        if (cur <= cap) {
            if (cur > before) {
                before = cur;
                pick = q;
            }
        } else if (!extra || cur < extraV) {
            extra = q;
            extraV = cur;
        }
    }
    if (!pick && extra) {
        pick = extra;
        before = extraV;
    }
    if (!pick || before < 0) return;
    int shown = before;
    if (before < floor) {
        *pick = floor;
        shown = floor;
    }
    // Mirror the counter the menu used to read only when it already tracked
    // the stack. A zero in the wrong field must not be overwritten.
    if (Readable(player + kEmblemHud, 8)) {
        uint8_t* hud = *(uint8_t**)(player + kEmblemHud);
        if (hud && Readable(hud + 0xC, 2)) {
            int hudCur = *(unsigned short*)(hud + 0xC);
            if (hudCur >= before - 3 && hudCur <= before + 3) WriteEmblemHud(player, shown, cap);
        }
    }
}

static int EmblemNow(uint8_t* player, int cap, int** slots, int* n) {
    *n = FindEmblemSlots(player, cap, slots);
    // The carried stack is the one at or under the cap. A higher row is storage
    // (or a Tanto overflow). Taking the max made a storage pile look full and
    // the reward never paid.
    int held = -1;
    int overflow = -1;
    for (int i = 0; i < *n; ++i) {
        if (!Readable(slots[i], 4)) continue;
        int v = *slots[i];
        if (v < 0 || v > cap + 15) continue;
        if (v <= cap) {
            if (v > held) held = v;
        } else if (overflow < 0 || v < overflow) {
            overflow = v;
        }
    }
    if (held >= 0) return held;
    return overflow;
}

static int EmblemCarry(uint8_t* player, int cap) {
    if (!player || cap < 1) return -1;
    int* slots[8];
    int n = 0;
    int cur = EmblemNow(player, cap, slots, &n);
    if (n < 1 || cur < 0) return -1;
    if (!g_emblemLogged) {
        g_emblemLogged = true;
        Logf("Spirit emblem stack found. Quantity %d.", cur);
    }
    return cur;
}

void PlayerFillEmblems() {
    if (!PlayerVersionOk()) return;
    uint8_t* player = Player();
    if (!player) return;
    int cap = EmblemCap(player);
    if (cap < 1) return;
    int* slots[8];
    int n = 0;
    int cur = EmblemNow(player, cap, slots, &n);
    if (n < 1 || cur < 0) {
        if (!g_emblemMiss) {
            g_emblemMiss = true;
            Log("Spirit emblem stack not found yet.");
        }
        return;
    }
    if (cur >= cap) return;
    g_emblemFloor = cap;
    WriteEmblemFloor();
    static int logged = -1;
    if (logged != cur) {
        logged = cur;
        Logf("Spirit emblems %d -> %d.", cur, cap);
    }
}

void PlayerAddEmblems(int count) {
    if (count <= 0 || !PlayerVersionOk()) return;
    uint8_t* player = Player();
    if (!player) return;
    int cap = EmblemCap(player);
    if (cap < 1) return;
    int* slots[8];
    int n = 0;
    int cur = EmblemNow(player, cap, slots, &n);
    if (n < 1 || cur < 0) {
        if (!g_emblemMiss) {
            g_emblemMiss = true;
            Log("Spirit emblem stack not found yet.");
        }
        return;
    }
    if (cur >= cap) return;
    int next = cur + count;
    if (next > cap) next = cap;
    if (next == cur) return;
    g_emblemFloor = next;
    g_emblemStableSince = 0;
    g_emblemHoldUntil = GetTickCount64() + kRewardHoldMs;
    WriteEmblemFloor();
    Logf("Spirit emblems %d -> %d.", cur, next);
}

static bool EmblemAtFloor() {
    uint8_t* player = Player();
    int cap = EmblemCap(player);
    if (!player || cap < 1) return false;
    int* slots[8];
    int n = 0;
    int cur = EmblemNow(player, cap, slots, &n);
    return cur >= 0 && cur >= g_emblemFloor;
}

void PlayerEnforceEmblems() {
    if (g_emblemHoldUntil == 0) return;
    ULONGLONG now = GetTickCount64();
    if (now >= g_emblemHoldUntil) {
        g_emblemHoldUntil = 0;
        g_emblemStableSince = 0;
        return;
    }
    if (EmblemAtFloor()) {
        if (g_emblemStableSince == 0) g_emblemStableSince = now;
        else if (now - g_emblemStableSince >= kRewardStableMs) {
            g_emblemHoldUntil = 0;
            g_emblemStableSince = 0;
        }
        return;
    }
    g_emblemStableSince = 0;
    WriteEmblemFloor();
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

static void RememberGourd(int* qty, int cap) {
    if (!qty || !Readable(qty, 4)) return;
    int cur = *qty;
    if (cur < 0 || cur > cap) return;
    for (int i = 0; i < g_gourdSlotN; ++i) if (g_gourdSlots[i] == qty) return;
    if (g_gourdSlotN >= 16) return;
    g_gourdSlots[g_gourdSlotN++] = qty;
}

static void CollectRange(uint8_t* base, size_t bytes, int cap) {
    if (!base || bytes < 8 || !Readable(base, bytes)) return;
    for (size_t i = 0; i + 8 <= bytes; i += 4) {
        if (*(unsigned*)(base + i) != kGourdItem) continue;
        RememberGourd((int*)(base + i + 4), cap);
    }
}

static void CollectList(uint8_t* player, int capOff, int headOff, int cap) {
    if (!Readable(player + capOff, 8)) return;
    int n = *(int*)(player + capOff);
    uint8_t* head = *(uint8_t**)(player + headOff);
    if (n < 1 || n > 2048 || !head || !Readable(head, (size_t)n * 0x10)) return;
    for (int i = 0; i < n; ++i) {
        uint8_t* e = head + (size_t)i * 0x10;
        if (*(unsigned*)(e + 4) == kGourdItem) RememberGourd((int*)(e + 8), cap);
    }
}

static void CollectGourd(int cap) {
    g_gourdSlotN = 0;
    uint8_t* player = Player();
    if (player) {
        CollectList(player, kNormalItemsCap, kNormalItems, cap);
        CollectList(player, kKeyItemsCap, kKeyItems, cap);
        if (Readable(player, 0xC00)) CollectRange(player, 0xC00, cap);
    }
    if (g_wolf && WolfId(g_wolf) && Readable(g_wolf, 0x3000)) CollectRange(g_wolf, 0x3000, cap);
    if (g_gourdQty) RememberGourd(g_gourdQty, cap);
}

static void WriteGourdFloor() {
    for (int i = 0; i < g_gourdSlotN; ++i) {
        int* q = g_gourdSlots[i];
        if (!Readable(q, 4)) continue;
        int cur = *q;
        if (cur < 0 || cur > g_gourdCap) continue;
        if (cur < g_gourdFloor) *q = g_gourdFloor;
    }
}

bool PlayerAddGourd(int charges, int cap) {
    if (charges <= 0) return false;
    if (cap < 0) cap = g_gourdCap;
    if (cap > 20) cap = 20;
    g_gourdCap = cap;
    CollectGourd(cap);
    int cur = -1;
    for (int i = 0; i < g_gourdSlotN; ++i) {
        if (!Readable(g_gourdSlots[i], 4)) continue;
        int v = *g_gourdSlots[i];
        if (v > cur) cur = v;
    }
    if (cur < 0) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            Log("Gourd charge had nowhere to write.");
        }
        return false;
    }
    int next = cur + charges;
    if (next > cap) next = cap;
    if (next == cur) return false;
    g_gourdFloor = next;
    g_gourdStableSince = 0;
    g_gourdHoldUntil = GetTickCount64() + kRewardHoldMs;
    WriteGourdFloor();
    g_gourdQty = g_gourdSlotN ? g_gourdSlots[0] : g_gourdQty;
    Logf("Gourd written to %d slot(s). Charges %d -> %d.", g_gourdSlotN, cur, next);
    return true;
}

static bool GourdAtFloor() {
    int seen = 0;
    for (int i = 0; i < g_gourdSlotN; ++i) {
        int* q = g_gourdSlots[i];
        if (!Readable(q, 4)) continue;
        int cur = *q;
        if (cur < 0 || cur > g_gourdCap) continue;
        seen += 1;
        if (cur < g_gourdFloor) return false;
    }
    return seen > 0;
}

static void ClearGourdHold() {
    g_gourdHoldUntil = 0;
    g_gourdStableSince = 0;
}

void PlayerEnforceGourd() {
    if (g_gourdHoldUntil == 0) return;
    ULONGLONG now = GetTickCount64();
    if (now >= g_gourdHoldUntil) {
        ClearGourdHold();
        return;
    }
    CollectGourd(g_gourdCap);
    if (GourdAtFloor()) {
        if (g_gourdStableSince == 0) g_gourdStableSince = now;
        else if (now - g_gourdStableSince >= kRewardStableMs) ClearGourdHold();
        return;
    }
    g_gourdStableSince = 0;
    WriteGourdFloor();
}

void PlayerFlushGourd() {
    PlayerEnforceGourd();
}

struct MeterScale {
    int baseMax;
    int appliedMax;
    int appliedTenths;
};

static MeterScale g_postureScale = {};

static int ScaledMeter(int base, int tenths) {
    if (tenths < 1) tenths = 10;
    long long v = ((long long)base * tenths + 5) / 10;
    if (v < 1) v = 1;
    if (v > 100000) v = 100000;
    return (int)v;
}

static void ApplyMeter(int* cur, int* mx, MeterScale* slot, int tenths) {
    if (!cur || !mx || !Readable(cur, 8)) return;
    int liveCur = *cur;
    int liveMax = *mx;
    if (liveMax < 1 || liveMax > 100000 || liveCur < -10 || liveCur > liveMax + 50) return;
    if (tenths < 1 || tenths > 1000) tenths = 10;
    if (tenths == 10) {
        if (slot->appliedMax > 0 && liveMax == slot->appliedMax && slot->baseMax > 0) {
            int restored = (int)(((long long)liveCur * slot->baseMax) / slot->appliedMax);
            if (liveCur >= liveMax) restored = slot->baseMax;
            if (restored < 0) restored = 0;
            if (restored > slot->baseMax) restored = slot->baseMax;
            *mx = slot->baseMax;
            *cur = restored;
        }
        slot->baseMax = 0;
        slot->appliedMax = 0;
        slot->appliedTenths = 10;
        return;
    }
    if (!(slot->appliedMax > 0 && liveMax == slot->appliedMax && slot->baseMax > 0))
        slot->baseMax = liveMax;
    int want = ScaledMeter(slot->baseMax, tenths);
    if (liveMax == want) {
        slot->appliedMax = want;
        slot->appliedTenths = tenths;
        return;
    }
    int nextCur = (int)(((long long)liveCur * want) / liveMax);
    if (liveCur >= liveMax) nextCur = want;
    if (nextCur < 0) nextCur = 0;
    if (nextCur > want) nextCur = want;
    *mx = want;
    *cur = nextCur;
    slot->appliedMax = want;
    slot->appliedTenths = tenths;
}

void PlayerApplyScales(int, int postureTenths) {
    if (!PlayerVersionOk()) return;
    // Vitality size is applied as less damage in the health hook. Writing a bigger max here
    // made Sekiro clamp current back to full on the next frame, which is immortality.
    uint8_t* player = Player();
    if (player && Readable(player + kPosture, 8))
        ApplyMeter((int*)(player + kPosture), (int*)(player + kMaxPosture), &g_postureScale, postureTenths);
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
