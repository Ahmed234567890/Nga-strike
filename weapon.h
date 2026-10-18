#ifndef WEAPON_H
#define WEAPON_H

#include "common.h"

void Weapon_Init(Weapon* weapons, int count);
// sprintBlend: 0..1 low-ready carry pose (weapon pulled down while running)
// moveRoll: viewmodel roll in degrees from strafing (negated by ADS outside)
// shotAge: seconds since the last shot (large when idle) -> per-shot cycling
void Weapon_DrawViewmodel(int index, Vector3 off, float muzzleTimer, float reloadT,
                          float adsBlend, float time, float sprintBlend, float moveRoll,
                          float shotAge);
void Weapon_DrawScopeOverlay(void);
void Weapon_DrawHitMarker(float timer, int type);
// reload staging: returns true once when crossing insert moment (for click sound)
bool Weapon_ReloadInsertCrossed(float prevT, float newT);
// empty reload takes longer (extra charge)
float Weapon_ReloadTime(const Weapon* w, bool empty);

#endif // WEAPON_H
