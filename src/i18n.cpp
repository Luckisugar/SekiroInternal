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
    {"everyblocks", "Blocks per charge", "Bloqueios por carga"},
    {"toward", "Toward next charge", "Ate a proxima carga"},
    {"guardnote", "Held blocks count toward this charge. A deflect uses the Parry row. Getting hit counts as neither.", "Bloqueio segurado conta para esta carga. Um aparo usa a linha Aparo. Tomar dano nao conta como nenhum dos dois."},
    {"watch", "Health watch", "Vigia de vida"},
    {"ghook", "Guard hook", "Gancho de guarda"},
    {"parrynote", "Easier parry and 100% parry are not in this build. The public no-damage flag makes enemies immune too.", "Aparo facil e aparo 100% nao estao nesta versao. A flag publica de sem dano deixa os inimigos imunes tambem."},
    {"cursed", "Sekiro has no cursed-energy stat. Spirit emblems are that resource.", "Sekiro nao tem energia amaldicoada. Os emblemas espirituais sao esse recurso."},
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
