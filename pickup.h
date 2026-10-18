#ifndef PICKUP_H
#define PICKUP_H

#include "common.h"
#include "audio.h"

void Pickup_Init(Pickup* pickups, int count);
void Pickup_Update(Pickup* pickups, int count, Player* player, Weapon* weapons, int wc,
                   Audio* audio, float dt, Particle* fx, int fxCount);
void Pickup_Draw(Pickup* pickups, int count, Vector3 camPos);

#endif
