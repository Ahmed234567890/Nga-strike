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
// a stub per remote peer) and, when bot infighting is enabled, one per living
// bot. Each enemy picks its own nearest visible target.
typedef struct {
    Player* player;
    Vector3 eye;       // target eye position used for line-of-sight/aim
    int     enemyId;   // >= 0 when this target is a bot: its enemies[] index
    bool    moving;    // anim/accuracy hints (from the owner's state)
    bool    airborne;
    bool    isLocal;   // hurt feedback (flash/indicator/sound) belongs to this machine
} EnemyTarget;

void Enemy_InitAll(Enemy* enemies, int count, const Vector3* spawns, int spawnCount);
// All-on: despawn everything and trickle the bots back in with staggered
// respawn timers. All-off: despawn everything and stop (no corpses, no spawns).
void Enemy_SetActiveAll(Enemy* enemies, int count, bool active);
void Enemy_UpdateAll(Enemy* enemies, int count, EnemyTarget* targets, int targetCount,
                     const Box* boxes, int boxCount, float mapLimit,
                     const Vector3* spawns, int spawnCount, Audio* audio, float dt,
                     DamageIndicator* dmgInds, int maxDmgInds,
                     float* damageFlashOut, float gameTime);
void Enemy_DrawAll(const Enemy* enemies, int count, Vector3 playerPos,
                   Vector3 camPos, Vector3 camFwd, float time, bool fakeBlobs);
void Enemy_AlertAll(Enemy* enemies, int count, Vector3 gunPos, float radius);

HitZone Enemy_ClassifyHit(const Enemy* e, Vector3 hitPos, bool headRayHit, float headDist, float bodyDist);
HitResult Enemy_ApplyDamage(Enemy* e, float dmg, HitZone zone);

BoundingBox Enemy_BodyBox(const Enemy* e);
BoundingBox Enemy_HeadBox(const Enemy* e);
BoundingBox Enemy_LegBox(const Enemy* e);

#endif
