#pragma once

#include <stdint.h>

struct Vitals {
    bool valid;
    int hp;
    int maxHp;
    int posture;
    int maxPosture;
    int emblems;
    int maxEmblems;
    int gourd;
    int gourdCap;
    bool combat;
};

bool PlayerVersionOk();
bool PlayerResolve(Vitals& out);
void PlayerNoteWolf(uint8_t* obj);
void PlayerKeepWolf(uint8_t* obj);
bool PlayerCombatSeen();
void PlayerAddHp(int add);
extern "C" int PlayerApplyHeal(uint8_t* body, int loaded, uint8_t* source);
void PlayerFillHp();
void PlayerFloorHp();
void PlayerFillEmblems();
void PlayerAddEmblems(int count);
void PlayerEnforceEmblems();
void PlayerRestorePosture(float pct);
bool PlayerAddGourd(int charges, int cap);
void PlayerFlushGourd();
void PlayerEnforceGourd();
void PlayerEnforceHp();
void PlayerSetNoDead(bool on);
void PlayerSetGourdCap(int cap);
void PlayerApplyScales(int healthTenths, int postureTenths);
