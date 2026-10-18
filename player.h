#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"
#include "world.h"

// Movement tuning (m/s and rates). Sprint is animation-driven in game.c via
// the horizontal speed Player_Update reports back.
#define PLAYER_WALK_SPEED    8.0f
#define PLAYER_SPRINT_SPEED  12.4f
#define PLAYER_ADS_SPEED     4.4f
#define PLAYER_STEP_UP       0.55f

void Player_Init(Player* p);
// sprintWant: Shift held (game.c gates on ADS etc). Reports horizontal
// speed + walk-bob into speedOut/bobOut; movement uses acceleration,
// stop friction, limited air control and auto step-up on curbs/stairs.
void Player_Update(Player* p, const Box* boxes, int boxCount, float yaw,
                   float dt, bool ads, bool sprintWant, Vector2* bobOut,
                   bool* movingOut, float* bobTime, float* speedOut);
void Player_Respawn(Player* p);
bool Player_TakeDamage(Player* p, float dmg);

// Blocky body silhouette drawn ONLY during the shadow caster pass so the
// player casts a real shadow on the ground (FPS: body itself is invisible).
void Player_DrawShadowBody(const Player* p, float yaw, float pitch);

// First-person legs: drawn in the viewmodel RT with the REAL world camera so
// they line up with the ground. Fades out while aiming down sights.
void Player_DrawLegs(const Player* p, float yaw, float bobTime, float speed,
                     bool grounded, float adsBlend);

#endif
