#pragma once

#include "../util/memory.h"
#include "weaponinfo.h"

class CBaseCombatWeapon {
public:
	constexpr WeaponInfo* GetWeaponData() {
		return memory::Call<WeaponInfo*>(this, 459); // maybe 459
	}
};
