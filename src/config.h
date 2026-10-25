#pragma once

#include <stdint.h>

// Sekiro patch 1.06. sekiro.exe is 68005144 bytes.
// GameDataMan and the debug flag block are the 1.06 addresses from the
// practice tool. Vitality layout matches the previous session's reads.

constexpr unsigned kSekiroExeBytes = 68005144u;
constexpr uintptr_t kGameDataManRva = 0x3D5AAC0;
constexpr uintptr_t kWorldChrManRva = 0x3D7A1E0;
constexpr uintptr_t kDebugFlagsRva = 0x3D7A369;
constexpr int kPlayerNoDeadDelta = -3;

constexpr int kInfo = 0x10;
constexpr int kHp = kInfo + 0x08;
constexpr int kMaxHp = kInfo + 0x0C;
constexpr int kPosture = kInfo + 0x24;
constexpr int kMaxPosture = kInfo + 0x28;
constexpr int kMaxEmblems = kInfo + 0x136;
constexpr int kCombatHp = 0x130;
constexpr unsigned kGourdItem = 0x40000BB8;

// PlayerGameData +0x278 is EquipGameData. EquipInventoryData is +0x168 inside
// that. Normal goods are a non-dense 0x10-byte list: id at +4, quantity at +8.
constexpr int kEquipInventory = 0x3E0;
constexpr int kNormalItemsCap = kEquipInventory + 0x14;
constexpr int kNormalItems = kEquipInventory + 0x18;
constexpr int kKeyItemsCap = kEquipInventory + 0x24;
constexpr int kKeyItems = kEquipInventory + 0x28;
