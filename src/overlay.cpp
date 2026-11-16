#include "overlay.h"
#include "life.h"
#include "player.h"
#include "arts.h"
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

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static HWND g_hwnd = nullptr;
static bool g_imgui = false;
static bool g_open = false;
static bool g_dead = false;
static bool g_insertWas = false;
static char g_iniUtf8[MAX_PATH * 3];

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

static bool ScaleSlider(const char* label, int* tenths) {
    int v = *tenths;
    if (v < 1) v = 1;
    if (v > 1000) v = 1000;
    char shown[16];
    sprintf_s(shown, "%d.%dx", v / 10, v % 10);
    bool edit = ImGui::SliderInt(label, &v, 1, 1000, shown, ImGuiSliderFlags_NoInput);
    if (v < 1) v = 1;
    if (v > 1000) v = 1000;
    if (v != *tenths) *tenths = v;
    return edit || v != *tenths;
}

static void RuleUi(const char* label, Rule* rule) {
    if (!ImGui::CollapsingHeader(label)) return;
    ImGui::PushID(label);
    bool edit = false;
    edit |= ImGui::SliderFloat(T("vit"), &rule->vitalityPercent, 0.f, 100.f, "%.2f");
    edit |= ImGui::SliderInt(T("every"), &rule->vitalityEvery, 1, 30);
    edit |= ImGui::SliderInt(T("emblems"), &rule->emblems, 0, 20);
    ImGui::PushID("ee");
    edit |= ImGui::SliderInt(T("every"), &rule->emblemsEvery, 1, 30);
    ImGui::PopID();
    edit |= ImGui::SliderFloat(T("posturepct"), &rule->posturePercent, 0.f, 100.f, "%.1f");
    ImGui::PushID("pe");
    edit |= ImGui::SliderInt(T("every"), &rule->postureEvery, 1, 30);
    ImGui::PopID();
    ImGui::PushID("gourd");
    edit |= ImGui::SliderFloat(T("gourdfrac"), &rule->gourdCharges, 0.f, 2.f, "%.2f");
    edit |= ImGui::SliderInt(T("every"), &rule->gourdEvery, 1, 30);
    ImGui::PopID();
    if (edit) LifeSave();
    ImGui::PopID();
}

static void ArtsRow(const char* label, int slot) {
    ImGui::PushID(slot);
    float chipW = 148.f;
    float gap = ImGui::GetStyle().ItemSpacing.x;
    float labelW = ImGui::GetContentRegionAvail().x - chipW - gap;
    if (labelW < 80.f) labelW = 80.f;
    if (ImGui::Button(label, ImVec2(labelW, 0))) ArtsRequest(slot);
    ImGui::SameLine();
    char chip[48];
    ArtsKeyLabel(slot, chip, (int)sizeof(chip));
    if (ImGui::Button(chip, ImVec2(chipW, 0))) ArtsBeginCapture(slot);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("chiphint"));
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
    ImGui::SetNextWindowSize(ImVec2(460, 680), ImGuiCond_FirstUseEver);
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
        ImGui::Text("%s  %d / %d", T("toward"), s->gourdProgress, s->blocksPerCharge);
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s", T("scalenote"));
    if (ScaleSlider(T("dmgx"), &s->damageTenths)) LifeSave();
    if (ScaleSlider(T("hpx"), &s->healthTenths)) LifeSave();
    if (ScaleSlider(T("posx"), &s->postureTenths)) LifeSave();
    ImGui::Separator();
    ArtsLoad();
    ImGui::TextWrapped("%s", T("artsnote"));
    ArtsRow(T("bolt"), ArtsBolt);
    ArtsRow(T("sakura"), ArtsSakura);
    const char* artsStatus = ArtsLastStatus();
    if (artsStatus[0]) ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.35f, 1.f), "%s", artsStatus);
    ImGui::Separator();

    if (ImGui::Checkbox(T("on"), &s->gourd)) LifeSave();
    if (ImGui::SliderInt(T("everyblocks"), &s->blocksPerCharge, 0, 20)) LifeSave();
    if (ImGui::SliderInt(T("gcap"), &s->gourdChargeCap, 0, 20)) LifeSave();
    ImGui::Separator();

    if (ImGui::Checkbox(T("infhp"), &s->infiniteHp)) LifeSave();
    if (ImGui::Checkbox(T("undeath"), &s->undeath)) LifeSave();
    if (ImGui::Checkbox(T("infemb"), &s->infiniteEmblems)) LifeSave();
    ImGui::Separator();

    if (ImGui::Checkbox(T("ls"), &s->lifesteal)) LifeSave();
    RuleUi(T("hit"), &s->rule[ActHit]);
    RuleUi(T("block"), &s->rule[ActBlock]);
    RuleUi(T("parry"), &s->rule[ActParry]);
    RuleUi(T("dreg"), &s->rule[ActDeath]);
    RuleUi(T("dmini"), &s->rule[ActDeathMini]);
    RuleUi(T("dboss"), &s->rule[ActDeathBoss]);
    if (ImGui::CollapsingHeader(T("tiers"))) {
        bool edit = false;
        edit |= ImGui::SliderInt(T("mini"), &s->miniBossVitalityAtLeast, 100, 5000);
        edit |= ImGui::SliderInt(T("boss"), &s->bossVitalityAtLeast, 100, 20000);
        if (edit) LifeSave();
    }
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
    // New view every frame. Holding the upscaler's back buffer across frames makes its resize fail.
    MakeRtv(sc);
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
    if (!g_open) {
        ArtsCancelCapture();
        ReleaseRtv();
        return;
    }
    DrawGuarded(sc);
}

// Called by SekiroPerf after it has upscaled, on the real output chain. This DLL does not
// patch the swap-chain vtable, so the upscaler keeps its own Present and ResizeBuffers.
extern "C" __declspec(dllexport) void __stdcall SekiroInternal_OnFrame(IDXGISwapChain* sc) {
    if (!sc) return;
    Frame(sc);
}
