#include "arts.h"
#include "config.h"
#include "player.h"
#include "i18n.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

// Caught bolt is speffect 9492, the effect a real airborne lightning hit applies
// (state 356, body vfx 6000). It is not Genichiro's leap. Sakura Dance is animation
// 316200 from Resurrection's c0000. ForceAnimation never asks if the art is unlocked
// or if an emblem can be spent.

static volatile LONG g_capture = -1;
static volatile LONG g_phase = 0;
static volatile LONG g_want[ArtsSlotCount] = {};
static volatile LONG g_pack[ArtsSlotCount] = {};
static bool g_wasDown[ArtsSlotCount] = {};
static bool g_seenUp[256] = {};
static bool g_loaded = false;
static bool g_fnChecked = false;
static bool g_fnOk = false;
static bool g_reservedLogged = false;
static volatile LONG g_note = 0;
static int g_lastSaid = 0;
static DWORD g_saidAt = 0;
static uint8_t* g_physSeen = nullptr;
static float g_groundY = 0.f;
static float g_lastY = 0.f;
static DWORD g_yTick = 0;
static bool g_yOk = false;
static volatile LONG g_air = 0;

enum {
    NoteNone = 0,
    NoteWolf,
    NoteExe,
    NoteGround,
    NoteNoMod,
    NoteArmed,
    NoteBoltFail,
    NoteSakura,
    NoteSakuraFail,
    NoteNoEvent
};

static const int kModCtrl = 1;
static const int kModAlt = 2;
static const int kModShift = 4;
static const float kAirMin = 0.05f;
static const float kAirMax = 20.f;

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

static unsigned Pack(int vk, int mods) {
    return (unsigned)(vk & 0xFF) | ((unsigned)(mods & 7) << 8);
}

static void Unpack(unsigned packed, int* vk, int* mods) {
    *vk = (int)(packed & 0xFF);
    *mods = (int)((packed >> 8) & 7);
}

static wchar_t* IniPath() {
    static wchar_t path[MAX_PATH];
    wcscpy_s(path, LogDir());
    wcscat_s(path, L"settings.ini");
    return path;
}

static bool Reserved(int vk) {
    return vk == VK_INSERT || vk == VK_F7 || vk == VK_F8;
}

static bool Modifier(int vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT
        || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
        || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU;
}

static int ModsNow() {
    int mods = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= kModCtrl;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= kModAlt;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= kModShift;
    return mods;
}

static void VkBody(int vk, char* out, int n) {
    if (vk >= '0' && vk <= '9') {
        out[0] = (char)vk;
        out[1] = 0;
        return;
    }
    if (vk >= 'A' && vk <= 'Z') {
        out[0] = (char)vk;
        out[1] = 0;
        return;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        sprintf_s(out, n, "Num %d", vk - VK_NUMPAD0);
        return;
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        sprintf_s(out, n, "F%d", vk - VK_F1 + 1);
        return;
    }
    const char* name = nullptr;
    switch (vk) {
    case VK_SPACE: name = "Space"; break;
    case VK_TAB: name = "Tab"; break;
    case VK_BACK: name = "Backspace"; break;
    case VK_RETURN: name = "Enter"; break;
    case VK_CAPITAL: name = "Caps Lock"; break;
    case VK_PRIOR: name = "Page Up"; break;
    case VK_NEXT: name = "Page Down"; break;
    case VK_END: name = "End"; break;
    case VK_HOME: name = "Home"; break;
    case VK_LEFT: name = "Left"; break;
    case VK_UP: name = "Up"; break;
    case VK_RIGHT: name = "Right"; break;
    case VK_DOWN: name = "Down"; break;
    case VK_DELETE: name = "Delete"; break;
    case VK_OEM_3: name = "`"; break;
    default: break;
    }
    if (name) sprintf_s(out, n, "%s", name);
    else sprintf_s(out, n, "key %d", vk);
}

static void StoreBind(int slot, int vk, int mods) {
    if (slot < 0 || slot >= ArtsSlotCount) return;
    InterlockedExchange(&g_pack[slot], (LONG)Pack(vk, mods));
    g_wasDown[slot] = true;
    wchar_t* ini = IniPath();
    wchar_t num[16];
    const wchar_t* vkName = slot == ArtsBolt ? L"boltVk" : L"sakuraVk";
    const wchar_t* modName = slot == ArtsBolt ? L"boltMods" : L"sakuraMods";
    swprintf_s(num, L"%d", vk);
    WritePrivateProfileStringW(L"arts", vkName, num, ini);
    swprintf_s(num, L"%d", mods);
    WritePrivateProfileStringW(L"arts", modName, num, ini);
}

void ArtsLoad() {
    if (g_loaded) return;
    g_loaded = true;
    wchar_t* ini = IniPath();
    int vk[ArtsSlotCount];
    int mods[ArtsSlotCount];
    vk[ArtsBolt] = GetPrivateProfileIntW(L"arts", L"boltVk", 0, ini);
    mods[ArtsBolt] = GetPrivateProfileIntW(L"arts", L"boltMods", 0, ini);
    vk[ArtsSakura] = GetPrivateProfileIntW(L"arts", L"sakuraVk", 0, ini);
    mods[ArtsSakura] = GetPrivateProfileIntW(L"arts", L"sakuraMods", 0, ini);
    for (int i = 0; i < ArtsSlotCount; ++i) {
        if (vk[i] < 0 || vk[i] > 255) vk[i] = 0;
        if (mods[i] < 0 || mods[i] > 7) mods[i] = 0;
        if (vk[i] == 0 || Reserved(vk[i]) || Modifier(vk[i])) {
            vk[i] = 0;
            mods[i] = 0;
        }
        InterlockedExchange(&g_pack[i], (LONG)Pack(vk[i], mods[i]));
    }
}

void ArtsRequest(int slot) {
    if (slot < 0 || slot >= ArtsSlotCount) return;
    InterlockedExchange(&g_want[slot], 1);
}

void ArtsBeginCapture(int slot) {
    if (slot < 0 || slot >= ArtsSlotCount) return;
    g_reservedLogged = false;
    g_phase = 0;
    InterlockedExchange(&g_capture, slot);
}

void ArtsCancelCapture() {
    InterlockedExchange(&g_capture, -1);
}

bool ArtsCapturing(int slot) {
    return g_capture == slot;
}

void ArtsKeyLabel(int slot, char* out, int outBytes) {
    if (!out || outBytes < 2) return;
    if (slot < 0 || slot >= ArtsSlotCount) {
        out[0] = 0;
        return;
    }
    if (g_capture == slot) {
        strncpy_s(out, outBytes, T("press"), _TRUNCATE);
        return;
    }
    int vk = 0;
    int mods = 0;
    Unpack((unsigned)g_pack[slot], &vk, &mods);
    if (vk <= 0) {
        strncpy_s(out, outBytes, T("none"), _TRUNCATE);
        return;
    }
    char body[24];
    VkBody(vk, body, sizeof(body));
    char prefix[40] = {};
    if (mods & kModCtrl) strcat_s(prefix, "Ctrl+");
    if (mods & kModAlt) strcat_s(prefix, "Alt+");
    if (mods & kModShift) strcat_s(prefix, "Shift+");
    sprintf_s(out, outBytes, "%s%s", prefix, body);
}

const char* ArtsLastStatus() {
    switch (g_note) {
    case NoteWolf: return T("artswolf");
    case NoteExe: return T("artsexe");
    case NoteGround: return T("boltground");
    case NoteNoMod: return T("boltnomod");
    case NoteArmed: return T("boltarmed");
    case NoteBoltFail: return T("boltfail");
    case NoteSakura: return T("sakuraok");
    case NoteSakuraFail: return T("sakurafail");
    case NoteNoEvent: return T("sakuramod");
    default: return "";
    }
}

static void Say(int code) {
    g_note = code;
    DWORD now = GetTickCount();
    if (code == g_lastSaid && now - g_saidAt < 500) return;
    g_lastSaid = code;
    g_saidAt = now;
    const char* text = ArtsLastStatus();
    if (text[0]) Log(text);
}

static bool FnsOk() {
    if (g_fnChecked) return g_fnOk;
    g_fnChecked = true;
    if (!PlayerVersionOk()) {
        Say(NoteExe);
        return false;
    }
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    if (!exe) return false;
    uint8_t* apply = (uint8_t*)exe + kApplySpEffectRva;
    uint8_t* dance = (uint8_t*)exe + kForceAnimEventRva;
    bool applyOk = Readable(apply, 3) && apply[0] == 0x48 && apply[1] == 0x8B && apply[2] == 0xC4;
    bool danceOk = Readable(dance, 4) && dance[0] == 0x40 && dance[1] == 0x53 && dance[2] == 0x48 && dance[3] == 0x83;
    g_fnOk = applyOk && danceOk;
    if (!g_fnOk) Say(NoteExe);
    return g_fnOk;
}

static uint8_t* ChrIns() {
    if (!PlayerVersionOk()) return nullptr;
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    if (!exe) return nullptr;
    uint8_t* slot = (uint8_t*)exe + kWorldChrManRva;
    if (!Readable(slot, sizeof(void*))) return nullptr;
    uint8_t* man = *(uint8_t**)slot;
    if (!Readable(man + kPlayerIns, sizeof(void*))) return nullptr;
    uint8_t* chr = *(uint8_t**)(man + kPlayerIns);
    if (!Readable(chr, (size_t)kChrEntityId + 4)) return nullptr;
    return chr;
}

static bool Owned(uint8_t* mod, uint8_t* chr) {
    if (!Readable(mod, (size_t)kAirTimer + 4)) return false;
    return *(uint8_t**)(mod + 8) == chr;
}

static float AirSeconds(uint8_t* mod) {
    return *(float*)(mod + kAirTimer);
}

static bool PlausibleAir(float timer) {
    return timer == timer && timer >= 0.f && timer <= kAirMax;
}

// ChrBehavior is the module the game adds to while Wolf is falling and zeroes on landing.
// Its +8 pointer is Wolf. Any other slot is left alone so a zero there cannot look like the ground.
static uint8_t* AirModule(uint8_t* chr) {
    if (!Readable(chr + kChrModules, sizeof(void*))) return nullptr;
    uint8_t* box = *(uint8_t**)(chr + kChrModules);
    if (!Readable(box + kChrBehavior, sizeof(void*))) return nullptr;
    uint8_t* behavior = *(uint8_t**)(box + kChrBehavior);
    if (!Owned(behavior, chr)) return nullptr;
    return behavior;
}

static int InvokeApply(void* chr, void* fn) {
    __try {
        ((void(__fastcall*)(void*, int, long long, long long))fn)(chr, kCaughtBoltEffect, 0, 0);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int InvokeDance(void* eventMod, void* fn) {
    __try {
        ((void(__fastcall*)(void*, int))fn)(eventMod, kSakuraAnim);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static uint8_t* ModuleAt(uint8_t* chr, int slot, size_t need) {
    if (!Readable(chr + kChrModules, sizeof(void*))) return nullptr;
    uint8_t* box = *(uint8_t**)(chr + kChrModules);
    if (!Readable(box + slot, sizeof(void*))) return nullptr;
    uint8_t* mod = *(uint8_t**)(box + slot);
    if (!mod || !Readable(mod, need)) return nullptr;
    return mod;
}

// The fall timer stays at 0 during a normal jump. Height off the last stable floor
// is what a hop actually changes. Long falls still count through the timer.
static void TrackAir() {
    uint8_t* chr = ChrIns();
    uint8_t* phys = chr ? ModuleAt(chr, kChrPhysics, (size_t)kPhysY + 4) : nullptr;
    if (!phys) {
        g_air = 0;
        g_yOk = false;
        g_physSeen = nullptr;
        return;
    }
    float y = *(float*)(phys + kPhysY);
    if (!(y == y) || y < -10000.f || y > 10000.f) {
        g_air = 0;
        return;
    }
    DWORD now = GetTickCount();
    if (!g_yOk || phys != g_physSeen) {
        g_physSeen = phys;
        g_yOk = true;
        g_groundY = y;
        g_lastY = y;
        g_yTick = now;
        g_air = 0;
        return;
    }
    float dt = (now - g_yTick) / 1000.f;
    g_yTick = now;
    if (dt < 0.008f) dt = 0.008f;
    if (dt > 0.1f) dt = 0.1f;
    float dy = y - g_lastY;
    g_lastY = y;
    float vy = dy / dt;
    float rise = y - g_groundY;
    bool falling = vy < -6.f;
    bool teleported = rise > 40.f || rise < -8.f;
    if (teleported && !falling) {
        g_groundY = y;
        g_air = 0;
        return;
    }
    bool onFloor = rise < 0.25f && rise > -0.20f && vy > -3.f && vy < 4.f;
    if (onFloor) g_groundY = y;
    bool timed = false;
    uint8_t* beh = AirModule(chr);
    if (beh) {
        float timer = AirSeconds(beh);
        timed = PlausibleAir(timer) && timer > kAirMin;
    }
    g_air = (rise > 0.35f || falling || timed) ? 1 : 0;
}

static void DoBolt() {
    if (!FnsOk()) return;
    uint8_t* chr = ChrIns();
    if (!chr) {
        Say(NoteWolf);
        return;
    }
    if (!g_air) {
        Say(g_yOk ? NoteGround : NoteNoMod);
        return;
    }
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    if (InvokeApply(chr, (uint8_t*)exe + kApplySpEffectRva)) Say(NoteArmed);
    else Say(NoteBoltFail);
}

static void DoSakura() {
    if (!FnsOk()) return;
    uint8_t* chr = ChrIns();
    if (!chr) {
        Say(NoteWolf);
        return;
    }
    uint8_t* eventMod = ModuleAt(chr, kChrEvent, 0x20);
    if (!eventMod) {
        Say(NoteNoEvent);
        return;
    }
    HMODULE exe = GetModuleHandleW(L"sekiro.exe");
    if (InvokeDance(eventMod, (uint8_t*)exe + kForceAnimEventRva)) Say(NoteSakura);
    else Say(NoteSakuraFail);
}

static void Drain() {
    if (InterlockedExchange(&g_want[ArtsBolt], 0)) DoBolt();
    if (InterlockedExchange(&g_want[ArtsSakura], 0)) DoSakura();
}

static bool ChordDown(int slot) {
    int vk = 0;
    int mods = 0;
    Unpack((unsigned)g_pack[slot], &vk, &mods);
    if (vk <= 0) return false;
    if ((GetAsyncKeyState(vk) & 0x8000) == 0) return false;
    return ModsNow() == mods;
}

static void PollHotkeys() {
    for (int i = 0; i < ArtsSlotCount; ++i) {
        bool down = ChordDown(i);
        if (down && !g_wasDown[i]) ArtsRequest(i);
        g_wasDown[i] = down;
    }
}

static void PollCapture() {
    int slot = (int)g_capture;
    if (slot < 0 || slot >= ArtsSlotCount) return;
    if (g_phase == 0) {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) return;
        memset(g_seenUp, 0, sizeof(g_seenUp));
        g_phase = 1;
        return;
    }
    for (int vk = 8; vk < 256; ++vk) {
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (!down) {
            g_seenUp[vk] = true;
            continue;
        }
        if (!g_seenUp[vk]) continue;
        if (Modifier(vk)) continue;
        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
        if (vk == VK_ESCAPE) {
            StoreBind(slot, 0, 0);
            InterlockedExchange(&g_capture, -1);
            return;
        }
        if (Reserved(vk)) {
            if (!g_reservedLogged) {
                g_reservedLogged = true;
                Log(T("artskey"));
            }
            continue;
        }
        StoreBind(slot, vk, ModsNow());
        InterlockedExchange(&g_capture, -1);
        return;
    }
}

void ArtsTick() {
    if (!g_loaded) ArtsLoad();
    TrackAir();
    if (g_capture >= 0) PollCapture();
    else PollHotkeys();
    Drain();
}
