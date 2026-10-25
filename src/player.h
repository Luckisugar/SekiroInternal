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
bool PlayerCombatSeen();
void PlayerAddHp(int add);
void PlayerFillHp();
void PlayerFloorHp();
void PlayerFillEmblems();
void PlayerAddEmblems(int count);
void PlayerRestorePosture(float pct);
bool PlayerAddGourd(int charges, int cap);
void PlayerFlushGourd();
void PlayerSetNoDead(bool on);
void PlayerSetGourdCap(int cap);
