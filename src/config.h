#pragma once

#include <stdint.h>

// Sekiro patch 1.06. sekiro.exe is 68005144 bytes.
// GameDataMan and the debug flag block are the 1.06 addresses from the
// practice tool. Vitality layout matches the previous session's reads.

constexpr unsigned kSekiroExeBytes = 68005144u;
constexpr uintptr_t kGameDataManRva = 0x3D5AAC0;
constexpr uintptr_t kWorldChrManRva = 0x3D7A1E0;
constexpr uintptr_t kMapItemManRva = 0x3D6CDC0;
constexpr uintptr_t kGrantItemRva = 0x91C970;
constexpr uintptr_t kDebugFlagsRva = 0x3D7A369;
constexpr int kPlayerNoDeadDelta = -3;

constexpr int kInfo = 0x10;
constexpr int kHp = kInfo + 0x08;
constexpr int kMaxHp = kInfo + 0x0C;
constexpr int kPosture = kInfo + 0x24;
constexpr int kMaxPosture = kInfo + 0x28;
constexpr int kMaxEmblems = kInfo + 0x136;
constexpr int kEmblemInv = 0x858;
constexpr int kEmblemHud = 0x190;
constexpr int kCombatHp = 0x130;
constexpr unsigned kGourdItem = 0x40000BB8;
constexpr unsigned kEmblemItem = 0x400003E8;

// PlayerGameData +0x278 is EquipGameData. EquipInventoryData is +0x168 inside
// that. Normal goods are a non-dense 0x10-byte list: id at +4, quantity at +8.
constexpr int kEquipInventory = 0x3E0;
constexpr int kNormalItemsCap = kEquipInventory + 0x14;
constexpr int kNormalItems = kEquipInventory + 0x18;
constexpr int kKeyItemsCap = kEquipInventory + 0x24;
constexpr int kKeyItems = kEquipInventory + 0x28;

// WorldChrMan+0x88 is the player ChrIns. ApplySpEffect takes that pointer and
// the effect id. ForceAnimation takes the entity id at ChrIns+0x1A24 and the
// animation number. The behavior module air timer at +0x8D0 is 0 on the ground.
constexpr int kPlayerIns = 0x88;
constexpr uintptr_t kApplySpEffectRva = 0xA01720;
constexpr uintptr_t kForceAnimationRva = 0x6D5A90;
constexpr int kChrModules = 0x1FF8;
constexpr int kChrBehavior = 0x28;
constexpr int kChrEvent = 0x58;
constexpr int kChrPhysics = 0x68;
constexpr int kPhysY = 0x84;
constexpr int kChrEntityId = 0x1A24;
constexpr int kAirTimer = 0x8D0;
constexpr uintptr_t kForceAnimEventRva = 0xBD84C0;
constexpr int kCaughtBoltEffect = 9492;
constexpr int kSakuraAnim = 316200;
