#ifndef COMMON_H
#define COMMON_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdbool.h>

// ---- Config ----
#define SCREEN_WIDTH         1280
#define SCREEN_HEIGHT        720
#define GROUND_SIZE          200.0f
#define MAP_LIMIT            48.0f
#define MOVE_SPEED           8.0f
#define MOUSE_SENS           0.003f
#define PLAYER_EYE_HEIGHT    2.0f
#define PLAYER_RADIUS        0.4f
#define PLAYER_MAX_HEALTH    100.0f
#define GRAVITY             -25.0f
#define JUMP_SPEED           9.0f
#define MUZZLE_FLASH_TIME    0.05f
#define ENEMY_MAX_HEALTH     100.0f
#define ENEMY_SPEED          2.8f
#define ENEMY_SHOOT_RANGE    20.0f
#define ENEMY_SHOOT_CD       1.35f
#define ENEMY_SHOOT_DMG      4.0f
#define ENEMY_AIM_TIME       0.6f
#define ENEMY_RESPAWN        4.0f
#define ENEMY_MELEE_RANGE    2.3f
#define ENEMY_MELEE_DMG      12.0f
#define ENEMY_MELEE_CD       1.0f
#define HITMARKER_TIME       0.32f
#define WEAPON_COUNT         3
#define MAX_ENEMIES          6

#define MAX_PICKUPS           8
#define MAX_DAMAGE_INDICATORS 8
#define MAX_DAMAGE_NUMBERS    16
#define MAX_PARTICLES         256
#define MAX_TRACERS           32
#define MAX_DECALS            64
#define PICKUP_RESPAWN_TIME   15.0f
#define PICKUP_HEALTH_AMOUNT  35.0f
#define PICKUP_AMMO_AMOUNT    45
#define PICKUP_RADIUS         1.4f
#define DAMAGE_INDICATOR_TIME 1.1f
#define DAMAGE_NUMBER_TIME    0.9f
#define MAX_BARRELS           6
#define BARREL_RADIUS         0.55f
#define BARREL_HEALTH         60.0f
#define BARREL_DAMAGE         170.0f
#define BARREL_BLAST          8.0f
#define BARREL_RESPAWN_TIME   18.0f

typedef struct {
    Vector3 position;   // feet
    Vector3 velocity;
    bool    grounded;
    float   health;
    float   spawnProtect; // seconds of damage immunity after (re)spawn
} Player;

typedef struct {
    Vector3 center;
    Vector3 halfExtents;
    Color   color;
} Box;

typedef struct {
    const char* name;
    int   magSize;
    int   ammoInMag;
    int   ammoReserve;
    int   reserveMax;
    float fireRate;
    float reloadTime;
    float bodyDamage;
    float headDamage;
    float recoilPitch;
    float recoilYaw;
    float recoilBack;
    float adsFov;
    float adsSens;
    float spreadHip;   // radians-ish cone
    float spreadAds;
    float range;
    bool  autoFire;
    bool  hasScope;
} Weapon;

typedef enum { PICKUP_AMMO, PICKUP_HEALTH } PickupType;

typedef struct {
    Vector3    position;
    PickupType type;
    bool       active;
    float      respawnTimer;
    float      bobTimer;
} Pickup;

typedef struct {
    Vector3 attackerPos;
    float   timer;
} DamageIndicator;

typedef struct {
    Vector3 position;
    float   damage;
    float   timer;
    int     type; // 0 body, 1 head, 2 kill
} DamageNumber;

typedef enum {
    E_PATROL = 0,
    E_CHASE  = 1,
    E_STRAFE = 2,
    E_COVER  = 3,
    E_FLEE   = 4,
} EnemyState;

typedef enum {
    ROLE_ASSAULT = 0,
    ROLE_MARKSMAN = 1,
    ROLE_RUSHER = 2,
} EnemyRole;

typedef enum {
    HIT_MISS = -1,
    HIT_TORSO = 0,
    HIT_HEAD = 1,
    HIT_LEG = 2,
    HIT_HELMET = 3,
} HitZone;

typedef struct {
    Vector3 position;   // center (y ~= 0.9)
    Vector3 spawnPoint;
    Vector3 homePos;
    Vector3 coverPos;
    bool    hasCoverPos;
    float   health;
    float   maxHealth;
    float   hurtTimer;
    float   flinchTimer;
    float   shootTimer;
    float   muzzleFlash;
    float   aimTimer;
    float   aimBlend;      // 0 = rifle at low ready, 1 = shouldered
    float   windupTimer;   // melee windup telegraph
    float   respawnTimer;
    float   deathTimer;    // corpse visible timer
    bool    alive;
    bool    hasHelmet;
    bool    elite;
    float   facing;        // yaw radians
    // AI
    EnemyState state;
    EnemyRole role;
    float   strafeDir;    // -1 / +1
    float   strafeTimer;
    int     burstLeft;
    float   burstTimer;
    float   meleeCd;
    float   alertTimer;   // >0 means player known
    float   visibleTimer; // continuous LOS time (reaction)
    float   walkPhase;
    float   speedMul;
    float   dmgMul;
    float   accMul;
    float   dodgeTimer;
    float   unstickTimer;
    Vector3 lastPos;
    float   stuckTimer;
    Vector3 lastSeenPos;   // where the player was last spotted (AI v2)
    float   suppressTimer; // suppression-fire cooldown (AI v2)
    int     seed;
} Enemy;

typedef struct {
    Vector3 position;   // feet center
    float   radius;
    float   health;
    bool    alive;
    float   respawnTimer;
    float   flash;
} Barrel;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float   life;
    float   maxLife;
    float   size0;
    float   size1;
    Color   color;
    float   gravity;
    float   drag;
} Particle;

typedef struct {
    Vector3 start;
    Vector3 end;
    float   timer;
    float   maxTime;
    Color   color;
} Tracer;

typedef struct {
    Vector3 position;
    float   timer;
    float   maxTime;
    float   size;
    Color   color;
} Decal;

// ---- Utils ----
static inline float RandomFloat(void) {
    return (float)GetRandomValue(0, 1000000) / 1000000.0f;
}

static inline float RandRange(float a, float b) {
    return a + (b - a) * RandomFloat();
}

static inline float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Blocky straight limb from a to b (forearms, arms). Used by the player
// viewmodel and the enemy soldiers so big obvious hands/arms read on screen.
static inline void DrawLimbBlocky(Vector3 a, Vector3 b, float th, Color col) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 0.02f) return;
    float yaw   = atan2f(d.x, d.z) * RAD2DEG;
    float pitch = -asinf(ClampF(d.y / len, -1.0f, 1.0f)) * RAD2DEG;
    rlPushMatrix();
    rlTranslatef(a.x, a.y, a.z);
    rlRotatef(yaw, 0, 1, 0);
    rlRotatef(pitch, 1, 0, 0);
    DrawCubeV((Vector3){ 0, 0, len * 0.5f }, (Vector3){ th, th, len }, col);
    rlPopMatrix();
}

static inline bool AABBvsAABB(Vector3 aPos, Vector3 aHalf, Vector3 bPos, Vector3 bHalf) {
    return (fabsf(aPos.x - bPos.x) < aHalf.x + bHalf.x) &&
           (fabsf(aPos.y - bPos.y) < aHalf.y + bHalf.y) &&
           (fabsf(aPos.z - bPos.z) < aHalf.z + bHalf.z);
}

static inline Vector2 WorldToScreen(Vector3 worldPos, Camera3D cam, bool* visible) {
    Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 right   = Vector3Normalize(Vector3CrossProduct(forward, cam.up));
    Vector3 up      = Vector3CrossProduct(right, forward);

    Vector3 toPoint = Vector3Subtract(worldPos, cam.position);

    float z = Vector3DotProduct(toPoint, forward);
    if (z <= 0.05f) {
        *visible = false;
        return (Vector2){ 0, 0 };
    }

    float x = Vector3DotProduct(toPoint, right);
    float y = Vector3DotProduct(toPoint, up);

    float fovRad = cam.fovy * DEG2RAD;
    float f = ((float)GetScreenHeight()) / (2.0f * tanf(fovRad * 0.5f));

    *visible = true;
    return (Vector2){
        (float)GetScreenWidth()  * 0.5f + (x / z) * f,
        (float)GetScreenHeight() * 0.5f - (y / z) * f
    };
}

static inline Vector3 EnemyBodyCenter(const Enemy* e) { return e->position; }
static inline Vector3 EnemyEyePos(const Enemy* e) {
    return (Vector3){ e->position.x, e->position.y + 0.55f, e->position.z };
}
static inline Vector3 PlayerEyePos(const Player* p) {
    return (Vector3){ p->position.x, p->position.y + PLAYER_EYE_HEIGHT, p->position.z };
}

#endif // COMMON_H
