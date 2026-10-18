#ifndef DAMAGE_FX_H
#define DAMAGE_FX_H

#include "common.h"

void DamageFX_Init(DamageIndicator* inds, int ic, DamageNumber* nums, int nc);
void DamageFX_Update(DamageIndicator* inds, DamageNumber* nums, int indCount, int numCount, float dt);
void DamageFX_SpawnIndicator(DamageIndicator* inds, int count, Vector3 attacker);
void DamageFX_SpawnNumber(DamageNumber* nums, int count, Vector3 worldPos, float damage, int type);
void DamageFX_DrawIndicators(DamageIndicator* inds, int count, Player* player, float yaw);
void DamageFX_DrawNumbers(DamageNumber* nums, int count, Camera3D camera);

#endif
