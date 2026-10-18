#ifndef ENEMY_H
#define ENEMY_H

#include "common.h"
#include "audio.h"

typedef struct {
    bool killed;
    bool helmetPopped;
    bool flinched;
} HitResult;

// Host/SP enemy brain targets: one entry per living player (the local one plus
// a stub per remote peer). Each enemy picks its own nearest visible target.
typedef struct {
    Player* player;
    bool    moving;    // anim/accuracy hints (from the owner's state)
    bool    airborne;
    bool    isLocal;   // hurt feedback (flash/indicator/sound) belongs to this machine
} EnemyTarget;

void Enemy_InitAll(Enemy* enemies, int count);
void Enemy_UpdateAll(Enemy* enemies, int count, EnemyTarget* targets, int targetCount,
                     const Box* boxes, int boxCount, Audio* audio, float dt,
                     DamageIndicator* dmgInds, int maxDmgInds,
                     float* damageFlashOut, float gameTime);
void Enemy_DrawAll(const Enemy* enemies, int count, Vector3 playerPos, float time, bool fakeBlobs);
void Enemy_AlertAll(Enemy* enemies, int count, Vector3 gunPos, float radius);

HitZone Enemy_ClassifyHit(const Enemy* e, Vector3 hitPos, bool headRayHit, float headDist, float bodyDist);
HitResult Enemy_ApplyDamage(Enemy* e, float dmg, HitZone zone);

BoundingBox Enemy_BodyBox(const Enemy* e);
BoundingBox Enemy_HeadBox(const Enemy* e);
BoundingBox Enemy_LegBox(const Enemy* e);

#endif
