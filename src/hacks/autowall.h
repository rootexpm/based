#pragma once

#include "../valve/cvector.h"
#include "../valve/centity.h"
#include "../valve/weaponinfo.h"

namespace Autowall {
    float CanHitDamage(const CVector& start, const CVector& end, CEntity* target = nullptr, WeaponInfo* weapon = nullptr);
}
