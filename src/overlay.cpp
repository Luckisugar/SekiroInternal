#include "overlay.h"
#include "life.h"
#include "player.h"
#include "i18n.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <stdio.h>
#include <string.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

struct VtHook {
    void** vt;
    PresentFn present;
    ResizeFn resize;
};

static VtHook g_vt[4];
static int g_vtN = 0;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static HWND g_hwnd = nullptr;
static bool g_imgui = false;
static bool g_open = false;
static bool g_dead = false;
static bool g_insertWas = false;
static volatile LONG g_installing = 0;
static thread_local int g_inPresent = 0;
static char g_iniUtf8[MAX_PATH * 3];

static PresentFn OrigPresent(IDXGISwapChain* sc) {
    void** vt = *(void***)sc;
    for (int i = 0; i < g_vtN; ++i) if (g_vt[i].vt == vt) return g_vt[i].present;
    return g_vtN ? g_vt[0].present : nullptr;
}

static ResizeFn OrigResize(IDXGISwapChain* sc) {
    void** vt = *(void***)sc;
    for (int i = 0; i < g_vtN; ++i) if (g_vt[i].vt == vt) return g_vt[i].resize;
    return g_vtN ? g_vt[0].resize : nullptr;
}

static void ReleaseRtv() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

static void MakeRtv(IDXGISwapChain* sc) {
    ReleaseRtv();
    ID3D11Texture2D* back = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) || !back) return;
    g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

static void RuleUi(const char* label, Rule* rule) {
    if (!ImGui::CollapsingHeader(label)) return;
    ImGui::PushID(label);
    bool edit = false;
    edit |= ImGui::SliderFloat(T("vit"), &rule->vitalityPercent, 0.f, 100.f, "%.2f");
    edit |= ImGui::SliderInt(T("every"), &rule->vitalityEvery, 1, 30);
    edit |= ImGui::SliderInt(T("emblems"), &rule->emblems, 0, 20);
    edit |= ImGui::SliderInt("##ee", &rule->emblemsEvery, 1, 30);
    edit |= ImGui::SliderFloat(T("posturepct"), &rule->posturePercent, 0.f, 100.f, "%.1f");
    ImGui::PushID("gourd");
    edit |= ImGui::SliderFloat(T("gourdfrac"), &rule->gourdCharges, 0.f, 2.f, "%.2f");
    edit |= ImGui::SliderInt(T("every"), &rule->gourdEvery, 1, 30);
    ImGui::PopID();
    if (edit) LifeSave();
    ImGui::PopID();
}

static void DrawMenu() {
    if (!LifeLanguageChosen()) {
        ImGui::SetNextWindowSize(ImVec2(360, 160), ImGuiCond_FirstUseEver);
        ImGui::Begin(T("lang"), nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", T("pick"));
        if (ImGui::Button(T("en"), ImVec2(140, 0))) LifeChooseLanguage(0);
        ImGui::SameLine();
        if (ImGui::Button(T("pt"), ImVec2(140, 0))) LifeChooseLanguage(1);
        ImGui::End();
        return;
    }

    LifeSettings* s = LifeGet();
    ImGui::SetNextWindowSize(ImVec2(460, 640), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sekiro", &g_open, ImGuiWindowFlags_NoCollapse);
    ImGui::TextDisabled("%s", T("insert"));
    int lang = I18nLang();
    if (ImGui::Combo(T("lang"), &lang, "English\0Portugues\0")) LifeChooseLanguage(lang);

    Vitals v{};
    bool stats = PlayerResolve(v);
    if (!stats) ImGui::TextWrapped("%s", T("statsbad"));
    else {
        ImGui::Text("%s  %d / %d", T("hp"), v.hp, v.maxHp);
        ImGui::Text("%s  %d / %d", T("pos"), v.posture, v.maxPosture);
        ImGui::Text("%s  %d / %d", T("emb"), v.emblems, v.maxEmblems);
        ImGui::Text("%s  %d / %d", T("gourd"), v.gourd, s->gourdChargeCap);
        int toward = s->rule[ActParry].gourdCharges > 0.f ? s->rule[ActParry].gourdEvery : s->blocksPerCharge;
        ImGui::Text("%s  %d / %d", T("toward"), s->gourdProgress, toward);
        ImGui::Text("%s  %s", T("combat"), v.combat ? T("seen") : T("waiting"));
    }
    ImGui::Text("%s  %s", T("watch"), LifeWatchNote());
    ImGui::Text("%s  %s", T("ghook"), LifeGuardNote());
    ImGui::Separator();

    if (ImGui::Checkbox(T("on"), &s->gourd)) LifeSave();
    if (ImGui::SliderInt(T("everyblocks"), &s->blocksPerCharge, 5, 10)) LifeSave();
    if (ImGui::SliderInt(T("gcap"), &s->gourdChargeCap, 1, 10)) LifeSave();
    ImGui::TextWrapped("%s", T("guardnote"));
    ImGui::Separator();

    if (ImGui::Checkbox(T("infhp"), &s->infiniteHp)) LifeSave();
    if (ImGui::Checkbox(T("undeath"), &s->undeath)) LifeSave();
    if (ImGui::Checkbox(T("infemb"), &s->infiniteEmblems)) LifeSave();
    ImGui::TextWrapped("%s", T("cursed"));
    ImGui::Separator();

    if (ImGui::Checkbox(T("ls"), &s->lifesteal)) LifeSave();
    RuleUi(T("hit"), &s->rule[ActHit]);
    RuleUi(T("block"), &s->rule[ActBlock]);
    RuleUi(T("parry"), &s->rule[ActParry]);
    RuleUi(T("dreg"), &s->rule[ActDeath]);
    RuleUi(T("dmini"), &s->rule[ActDeathMini]);
    RuleUi(T("dboss"), &s->rule[ActDeathBoss]);
    if (ImGui::CollapsingHeader(T("tiers"))) {
        ImGui::SliderInt(T("mini"), &s->miniBossVitalityAtLeast, 100, 5000);
        ImGui::SliderInt(T("boss"), &s->bossVitalityAtLeast, 100, 20000);
        if (ImGui::IsItemDeactivatedAfterEdit()) LifeSave();
    }
    ImGui::TextWrapped("%s", T("parrynote"));
    ImGui::Separator();
    if (ImGui::CollapsingHeader(T("activity"), ImGuiTreeNodeFlags_DefaultOpen)) {
        static char pane[6000];
        LogCopy(pane, sizeof(pane), 18);
        ImGui::BeginChild("log", ImVec2(0, 160), true);
        ImGui::TextUnformatted(pane);
        ImGui::EndChild();
    }
    ImGui::End();
}

static void DrawOverlay(IDXGISwapChain* sc) {
    if (g_dead) return;
    if (!g_imgui) {
        if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_device)) || !g_device) return;
        g_device->GetImmediateContext(&g_context);
        DXGI_SWAP_CHAIN_DESC desc{};
        sc->GetDesc(&desc);
        g_hwnd = desc.OutputWindow;
        MakeRtv(sc);
        if (!g_rtv || !g_hwnd) return;
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        wchar_t ini[MAX_PATH];
        wcscpy_s(ini, LogDir());
        wcscat_s(ini, L"imgui.ini");
        WideCharToMultiByte(CP_UTF8, 0, ini, -1, g_iniUtf8, sizeof(g_iniUtf8), nullptr, nullptr);
        io.IniFilename = g_iniUtf8;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        ImGui::StyleColorsDark();
        ImVec4* c = ImGui::GetStyle().Colors;
        c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.05f, 0.05f, 0.94f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.42f, 0.08f, 0.08f, 1.f);
        c[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.28f, 0.22f, 1.f);
        c[ImGuiCol_SliderGrab] = ImVec4(0.62f, 0.14f, 0.12f, 1.f);
        c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.10f, 0.10f, 1.f);
        c[ImGuiCol_Header] = ImVec4(0.38f, 0.10f, 0.10f, 0.80f);
        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_device, g_context);
        g_imgui = true;
        Log("Menu ready.");
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    POINT cursor{};
    if (GetCursorPos(&cursor) && ScreenToClient(g_hwnd, &cursor)) io.AddMousePosEvent((float)cursor.x, (float)cursor.y);
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    io.MouseDrawCursor = true;
    ImGui::NewFrame();
    DrawMenu();
    ImGui::Render();
    if (g_rtv) {
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

static void DrawGuarded(IDXGISwapChain* sc) {
    if (g_dead) return;
    __try {
        DrawOverlay(sc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_dead = true;
        LogFail("The menu faulted and stopped drawing. The game was left running.");
    }
}

static void Frame(IDXGISwapChain* sc) {
    bool down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (down && !g_insertWas) g_open = !g_open;
    g_insertWas = down;
    LifeTick();
    if (g_open) DrawGuarded(sc);
}

static HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    PresentFn orig = OrigPresent(sc);
    if (!orig) return E_FAIL;
    if (g_installing || g_inPresent) return orig(sc, sync, flags);
    g_inPresent = 1;
    // Draw into the back buffer before Present. Drawing after Present lands on
    // the next buffer, and the game clears that before it is ever shown.
    Frame(sc);
    HRESULT hr = orig(sc, sync, flags);
    g_inPresent = 0;
    return hr;
}

static HRESULT __stdcall hkResize(IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT fl) {
    ReleaseRtv();
    ResizeFn orig = OrigResize(sc);
    HRESULT hr = orig ? orig(sc, count, w, h, fmt, fl) : E_FAIL;
    if (g_device) MakeRtv(sc);
    return hr;
}

static void PatchVt(IDXGISwapChain* sc) {
    if (!sc || g_vtN >= 4) return;
    void** vt = *(void***)sc;
    for (int i = 0; i < g_vtN; ++i) if (g_vt[i].vt == vt) return;
    DWORD old = 0;
    if (!VirtualProtect(&vt[8], sizeof(void*) * 6, PAGE_EXECUTE_READWRITE, &old)) return;
    VtHook* hook = &g_vt[g_vtN++];
    hook->vt = vt;
    hook->present = (PresentFn)vt[8];
    hook->resize = (ResizeFn)vt[13];
    vt[8] = (void*)hkPresent;
    vt[13] = (void*)hkResize;
    VirtualProtect(&vt[8], sizeof(void*) * 6, old, &old);
}

static bool HookEffect(DXGI_SWAP_EFFECT effect, UINT buffers) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SekiroInternalDummy";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) return false;
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = buffers;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = effect;
    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL level{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &sc, &dev, &level, &ctx);
    bool ok = SUCCEEDED(hr) && sc;
    if (ok) PatchVt(sc);
    if (sc) sc->Release();
    if (ctx) ctx->Release();
    if (dev) dev->Release();
    DestroyWindow(hwnd);
    return ok;
}

bool OverlayInstall() {
    InterlockedExchange(&g_installing, 1);
    bool a = HookEffect(DXGI_SWAP_EFFECT_DISCARD, 1);
    bool b = HookEffect(DXGI_SWAP_EFFECT_FLIP_DISCARD, 2);
    InterlockedExchange(&g_installing, 0);
    if (!a && !b) {
        LogFail("Could not create a D3D11 swap chain to hook Present.");
        return false;
    }
    Logf("Present hook installed on %d swap-chain type(s).", g_vtN);
    return g_vtN > 0;
}
