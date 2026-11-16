#include "i18n.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

static int g_lang = 0;
static wchar_t g_path[MAX_PATH];

struct Row {
    const char* id;
    const char* en;
    const char* pt;
};

static const Row kRows[] = {
    {"loaded", "SekiroInternal loaded.", "SekiroInternal carregou."},
    {"insert", "Insert opens and closes this menu.", "Insert abre e fecha este menu."},
    {"lang", "Language", "Idioma"},
    {"wolf", "Wolf", "Wolf"},
    {"infhp", "Infinite vitality", "Vitalidade infinita"},
    {"undeath", "Undeath (never below 1)", "Imortal (nunca abaixo de 1)"},
    {"infemb", "Infinite spirit emblems", "Emblemas espirituais infinitos"},
    {"hp", "Vitality", "Vitalidade"},
    {"dmgx", "Damage dealt", "Dano causado"},
    {"hpx", "Vitality size", "Tamanho da vitalidade"},
    {"posx", "Posture size", "Tamanho da postura"},
    {"scalenote", "1.0x is normal. Steps of 0.1, up to 100.0x.", "1.0x e o normal. Passos de 0.1, ate 100.0x."},
    {"artsnote", "Catch lightning in the air, then attack before you land. Sakura Dance needs no unlock and spends no emblems.", "Pegue o raio no ar e ataque antes de cair. A Danca da Sakura nao precisa de desbloqueio e nao gasta emblemas."},
    {"bolt", "Catch lightning", "Pegar raio"},
    {"sakura", "Sakura Dance", "Danca da Sakura"},
    {"press", "press a key", "aperte uma tecla"},
    {"none", "none", "nenhuma"},
    {"chiphint", "Click, then press a key. Hold Ctrl, Alt, or Shift for a chord. Escape clears it.", "Clique e aperte uma tecla. Segure Ctrl, Alt ou Shift para um atalho. Esc limpa."},
    {"boltground", "Still on the ground. Lightning catch only arms in the air.", "Ainda no chao. O raio so e pego no ar."},
    {"boltnomod", "Air timer was not readable. Lightning catch was not applied.", "O tempo no ar nao pode ser lido. O raio nao foi aplicado."},
    {"boltarmed", "Lightning catch armed. Swing before you land.", "Raio pego. Golpeie antes de cair."},
    {"boltfail", "Lightning catch call failed.", "A chamada do raio falhou."},
    {"sakuraok", "Sakura Dance started.", "Danca da Sakura comecou."},
    {"sakurafail", "Sakura Dance call failed.", "A chamada da Danca da Sakura falhou."},
    {"sakuramod", "Sakura Dance could not find the animation module.", "A Danca da Sakura nao achou o modulo de animacao."},
    {"artswolf", "Wolf is not in the world yet.", "O Wolf ainda nao esta no mundo."},
    {"artsexe", "Game functions did not match patch 1.06. Nothing was called.", "As funcoes do jogo nao bateram com a patch 1.06. Nada foi chamado."},
    {"artskey", "Insert, F7, and F8 stay with the menu and the other mods.", "Insert, F7 e F8 ficam com o menu e os outros mods."},
    {"emb", "Spirit emblems", "Emblemas espirituais"},
    {"pos", "Posture", "Postura"},
    {"gourd", "Gourd", "Gourd"},
    {"combat", "Combat health", "Vida de combate"},
    {"seen", "seen", "visto"},
    {"waiting", "not seen yet", "ainda nao visto"},
    {"ls", "Lifesteal", "Roubo de vida"},
    {"hit", "Landed hit", "Golpe acertado"},
    {"block", "Held block", "Bloqueio segurado"},
    {"parry", "Parry", "Aparo"},
    {"dreg", "Deathblow", "Deathblow"},
    {"dmini", "Mini-boss deathblow", "Deathblow de mini-chefe"},
    {"dboss", "Boss deathblow", "Deathblow de chefe"},
    {"vit", "Vitality % of max", "Vitalidade % do maximo"},
    {"emblems", "Emblems", "Emblemas"},
    {"every", "Every N", "A cada N"},
    {"posturepct", "Posture % restored", "Postura % restaurada"},
    {"gourdfrac", "Gourd fraction", "Fracao do gourd"},
    {"tiers", "Tiers", "Faixas"},
    {"parryat", "Parry if posture rises by at most", "Aparo se a postura sobe no maximo"},
    {"mini", "Mini-boss if max vitality is at least", "Mini-chefe se a vitalidade maxima for pelo menos"},
    {"boss", "Boss if max vitality is at least", "Chefe se a vitalidade maxima for pelo menos"},
    {"gcap", "Gourd charge cap", "Teto de cargas do gourd"},
    {"on", "Restore gourd", "Restaurar gourd"},
    {"everyblocks", "Parries per charge", "Aparos por carga"},
    {"toward", "Toward next charge", "Ate a proxima carga"},
    {"activity", "Activity", "Atividade"},
    {"statsbad", "Wolf's stats are not readable yet. Normal on the title screen.", "A vida do Wolf ainda nao pode ser lida. Normal na tela de titulo."},
    {"versionbad", "sekiro.exe is not the 1.06 build this DLL knows. No stats will be written.", "sekiro.exe nao e a build 1.06 que esta DLL conhece. Nenhum status sera escrito."},
    {"pick", "Pick a language. You can change it in the menu.", "Escolha o idioma. Da para mudar no menu."},
    {"en", "English", "English"},
    {"pt", "Portugues", "Portugues"},
};

const wchar_t* I18nPath() {
    if (!g_path[0]) {
        wcscpy_s(g_path, LogDir());
        wcscat_s(g_path, L"ui-language.txt");
    }
    return g_path;
}

static void SaveLang() {
    FILE* f = nullptr;
    _wfopen_s(&f, I18nPath(), L"wb");
    if (!f) return;
    fputs(g_lang == 1 ? "pt\n" : "en\n", f);
    fclose(f);
}

void I18nSet(int lang) {
    g_lang = lang == 1 ? 1 : 0;
    SaveLang();
}

int I18nLang() { return g_lang; }

void I18nLoad() {
    I18nPath();
    FILE* f = nullptr;
    _wfopen_s(&f, g_path, L"rb");
    if (!f) return;
    char buf[16] = {};
    if (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "pt", 2) == 0) g_lang = 1;
    }
    fclose(f);
}

const char* T(const char* id) {
    for (const Row& row : kRows) {
        if (strcmp(row.id, id) == 0) return g_lang == 1 ? row.pt : row.en;
    }
    return id;
}
