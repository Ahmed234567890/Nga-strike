#ifndef GAME_H
#define GAME_H

#include "common.h"
#include "audio.h"
#include "world.h"
#include "shadows.h"
#include "settings.h"
#include "net.h"

typedef struct {
    Player player;
    Weapon weapons[WEAPON_COUNT];
    int curWeapon;
    WorldMap world;
    uint32_t worldEpoch;
    Enemy enemies[MAX_ENEMIES];
    Barrel barrels[MAX_BARRELS];
    Pickup pickups[MAX_PICKUPS];
    DamageIndicator dmgInds[MAX_DAMAGE_INDICATORS];
    DamageNumber dmgNums[MAX_DAMAGE_NUMBERS];
    Particle particles[MAX_PARTICLES];
    Tracer tracers[MAX_TRACERS];
    Decal decals[MAX_DECALS];
    int decalCursor;
    Audio audio;
    ShadowMap shadow;
    Camera3D camera;
    Camera3D viewCam;
    RenderTexture2D viewRT;
    bool viewRTok;
    float yaw, pitch;
    float adsBlend;
    bool adsActive;
    Vector2 sway;
    Vector2 bob;
    float bobTime;
    float footTimer;
    bool moving;
    float moveSpeed;     // horizontal speed (m/s) reported by Player_Update
    float sprintBlend;   // 0..1 sprint carry blend (fov kick, viewmodel pose)
    float recoilBack;
    float fireCd;
    float muzzleTimer;
    bool reloading;
    float reloadTimer;
    float reloadTotal;
    float reloadPrevT;
    bool reloadPlayedInsert;
    bool reloadEmpty;
    float switchTimer;
    Vector3 lastHitPos;
    float lastHitTimer;
    float damageFlash;
    float hitMarkerTimer;
    int hitMarkerType;
    float headshotBanner;
    int headshotStreak;
    float headshotStreakTimer;
    float trauma; // camera shake
    float dustTimer;
    bool paused;
    bool shouldQuit;
    int kills;
    int score;
    int shots;
    int hits;
    int headshots;
    int helmetPops;
    float gameTime;
    float sens;
    float masterVol;
    Settings set;
    float landDip;   // viewmodel dip on landing
    // v3.1: static lamp positions cached at bake time (dynamic actor shadows
    // re-select the two nearest of these every frame) + explosion blast light
    Vector3 lampPos[SHADOW_MAX_BAKED_LAMPS];
    int lampCount;
    float boomTimer;
    Vector3 boomPos;
    // LAN multiplayer state (see net.h): NET_OFF keeps the whole game exactly
    // as single-player
    NetState net;
} Game;

void Game_Init(Game* g);
void Game_Update(Game* g, float dt);
void Game_Draw(Game* g);
void Game_Unload(Game* g);

// Toggle the AI bots at runtime: off despawns every enemy and stops spawns,
// on brings them back on staggered respawn timers.
void Game_SetAIActive(Game* g, bool active);

// Rebuild the arena and reset the round. Used by NEW MAP (host/single-player)
// and by clients when the host's map epoch changes.
void Game_NewMap(Game* g, WorldMode mode, uint32_t seed);
void Game_ApplyRemoteWorld(Game* g, int mode, uint32_t seed, uint32_t epoch);

#endif
