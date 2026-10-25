#pragma once

enum ActionId {
    ActHit = 0,
    ActBlock,
    ActParry,
    ActDeath,
    ActDeathMini,
    ActDeathBoss,
    ActCount
};

struct Rule {
    float vitalityPercent;
    int vitalityEvery;
    int emblems;
    int emblemsEvery;
    float posturePercent;
    int postureEvery;
    float gourdCharges;
    int gourdEvery;
};

struct LifeSettings {
    bool infiniteHp;
    bool undeath;
    bool infiniteEmblems;
    bool lifesteal;
    bool gourd;
    int blocksPerCharge;
    int gourdProgress;
    Rule rule[ActCount];
    int parryIfPostureGainAtMost;
    int miniBossVitalityAtLeast;
    int bossVitalityAtLeast;
    int gourdChargeCap;
};

LifeSettings* LifeGet();
void LifeLoad();
void LifeSave();
void LifeTick();
void LifeEnsureWatch();
void LifeEnsureGuard();
const char* LifeWatchNote();
const char* LifeGuardNote();
bool LifeLanguageChosen();
void LifeChooseLanguage(int lang);
