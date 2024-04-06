//
// Created by mikolaj on 12.06.22.
//
#pragma once

#include "base/vmath.h"

struct AvailableCheats{
    bool Godmode= false;
    bool AllWeapons= false;
    bool FullAuto= false;
    bool NoSelfDamage= false;
    bool NoEnemyDamage= false;
    bool LockMovement = false;
    bool LockHook = false;
    bool LockPosition = false;
    vec2 LockPos;
    bool LockWeapons = false;
    bool Jetpack = false;
    bool SuperHook = false;
    bool SuperNinja= false;
};