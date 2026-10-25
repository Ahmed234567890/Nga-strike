#include "game.h"
#include "player.h"
#include "weapon.h"
#include "enemy.h"
#include "pickup.h"
#include "damage_fx.h"
#include "fx.h"
#include "ui.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Vector3 ShotDir(const Game* g, const Weapon* w, Vector3 forward) {
    float spread = w->spreadHip + (w->spreadAds - w->spreadHip) * g->adsBlend;
    if (g->moving) spread *= 1.6f;
    if (g->sprintBlend > 0.3f) spread *= 1.5f;
    if (!g->player.grounded) spread *= 1.8f;
    if (g->muzzleTimer > 0) spread *= 1.25f;
    Vector3 up = { 0, 1, 0 };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, up));
    Vector3 realUp = Vector3CrossProduct(right, forward);
    float a = RandomFloat() * 2.0f * PI;
    float r = sqrtf(RandomFloat()) * spread;
    Vector3 d = Vector3Add(forward, Vector3Scale(right, cosf(a) * r));
    d = Vector3Add(d, Vector3Scale(realUp, sinf(a) * r));
    return Vector3Normalize(d);
}

static void Barrels_Init(Barrel* b, const Vector3* spots, int n) {
    if (n > MAX_BARRELS) n = MAX_BARRELS;
    for (int i = 0; i < MAX_BARRELS; i++) {
        b[i].position = (Vector3){ 0, -1000.0f, 0 };
        b[i].radius = BARREL_RADIUS;
        b[i].health = BARREL_HEALTH;
        b[i].alive = (i < n && spots);
        if (b[i].alive) b[i].position = spots[i];
        b[i].respawnTimer = b[i].alive ? 0.0f : 1e9f;
        b[i].flash = 0;
    }
}

static void Game_BakeLampShadows(Game* g);

static void Game_ResetRoundState(Game* g, int spawnIndex);

static void Game_PlaceLocalPlayer(Game* g, int spawnIndex) {
    int n = WORLD_MAX_PLAYER_SPAWNS;
    if (spawnIndex < 0 || spawnIndex >= n) spawnIndex = 0;
    Vector3 sp = Net_SpawnPos(spawnIndex);
    if (!isfinite(sp.x) || !isfinite(sp.z) ||
        fabsf(sp.x) > g->world.limit + 2.0f || fabsf(sp.z) > g->world.limit + 2.0f) {
        sp = g->world.playerSpawns[spawnIndex];
    }
    sp.y = 0.0f;
    g->player.position = sp;
    g->player.velocity = (Vector3){ 0, 0, 0 };
    g->player.health = PLAYER_MAX_HEALTH;
    g->player.grounded = true;
    g->player.spawnProtect = 2.0f;
    g->yaw = Net_SpawnYaw(spawnIndex);
    g->pitch = 0;
    g->bob = (Vector2){ 0, 0 };
    g->bobTime = 0;
    g->adsBlend = 0;
    g->sprintBlend = 0;
    g->reloading = false;
    g->reloadTimer = 0;
    g->fireCd = 0;
    g->switchTimer = 0;
    g->muzzleTimer = 0;
    g->recoilBack = 0;
    g->landDip = 0;
    g->trauma = 0;
}

static void Game_LoadWorldEntities(Game* g) {
    Enemy_InitAll(g->enemies, MAX_ENEMIES, g->world.enemySpawns, g->world.enemySpawnCount);
    Barrels_Init(g->barrels, g->world.barrelSpots, g->world.barrelCount);
    Pickup_Init(g->pickups, g->world.pickupCount, g->world.pickupSpots, g->world.pickupTypes);
    // Clients mirror the host's enemy state, so keep the locally-generated
    // proxies dormant until the next snapshot (or welcome) fills them in.
    if (!g->set.aiEnabled || g->net.mode == NET_CLIENT)
        Enemy_SetActiveAll(g->enemies, MAX_ENEMIES, false);
    Net_SetSpawnLayout(g->world.playerSpawns, WORLD_MAX_PLAYER_SPAWNS);
}

void Game_Init(Game* g, const Settings* initialSettings) {
    memset(g, 0, sizeof(*g));
    if (initialSettings) g->set = *initialSettings;
    else Settings_Load(&g->set, "nga.cfg");
    g->nextWorldMode = (WorldMode)g->set.worldMode;
    Player_Init(&g->player);
    Weapon_Init(g->weapons, WEAPON_COUNT);
    g->curWeapon = 0;
    World_Generate(&g->world, (WorldMode)g->set.worldMode, g->set.worldSeed);
    g->worldEpoch = 1;
    DamageFX_Init(g->dmgInds, MAX_DAMAGE_INDICATORS, g->dmgNums, MAX_DAMAGE_NUMBERS);
    FX_Init(g->particles, MAX_PARTICLES, g->tracers, MAX_TRACERS, g->decals, MAX_DECALS);
    Audio_Init(&g->audio);
    Shadow_Init(&g->shadow);

    g->camera.position = (Vector3){ 0, PLAYER_EYE_HEIGHT, 0 };
    g->camera.up = (Vector3){ 0, 1, 0 };
    g->camera.fovy = 70;
    g->camera.projection = CAMERA_PERSPECTIVE;

    g->viewCam.position = (Vector3){ 0, 0, 0 };
    g->viewCam.target = (Vector3){ 0, 0, -1 };
    g->viewCam.up = (Vector3){ 0, 1, 0 };
    g->viewCam.fovy = 70;
    g->viewCam.projection = CAMERA_PERSPECTIVE;

    g->viewRT = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    g->viewRTok = (g->viewRT.id > 0);
    if (g->viewRTok) SetTextureFilter(g->viewRT.texture, TEXTURE_FILTER_BILINEAR);

    // Depth precision: default near=0.01/far=1000 makes layered ground
    // (0.02-0.06 apart) z-fight and pop at 30-50m. 0.08/400 is stable.
    rlSetClipPlanes(0.08, 400.0);

    g->sens = MOUSE_SENS;
    g->masterVol = 1.0f;
    g->footTimer = 0.35f;

    // LAN multiplayer bring-up (no-op when launched without --host/--join)
    Net_Init(&g->net);
    Net_SetWorld(&g->net, (uint8_t)g->world.mode, g->world.seed, g->worldEpoch,
                 World_LayoutHash(&g->world));
    Game_LoadWorldEntities(g);
    Game_BakeLampShadows(g);

    if (g->net.mode == NET_CLIENT) {
        // world state (enemies/barrels/pickups) comes from the host snapshots
        for (int i = 0; i < MAX_ENEMIES; i++) g->enemies[i].alive = false;
    }
    Game_ResetRoundState(g, 0);
}

void Game_SetAIActive(Game* g, bool active) {
    g->set.aiEnabled = active;
    Enemy_SetActiveAll(g->enemies, MAX_ENEMIES, active);
    TraceLog(LOG_INFO, "AI enemies: %s", active ? "on" : "off");
}

static int Game_LocalSpawnIndex(const Game* g) {
    if (g->net.mode == NET_CLIENT && g->net.myId > 0 && g->net.myId <= WORLD_MAX_PLAYER_SPAWNS)
        return g->net.myId - 1;
    return 0;
}

static void Game_ResetRoundState(Game* g, int spawnIndex) {
    Weapon_Init(g->weapons, WEAPON_COUNT);
    g->curWeapon = 0;
    Player_Init(&g->player);
    Game_PlaceLocalPlayer(g, spawnIndex);

    g->fireCd = 0;
    g->muzzleTimer = 0;
    g->reloading = false;
    g->reloadTimer = 0;
    g->reloadTotal = 0;
    g->reloadPrevT = 0;
    g->reloadPlayedInsert = false;
    g->reloadEmpty = false;
    g->switchTimer = 0;
    g->adsActive = false;
    g->adsBlend = 0;
    g->sprintBlend = 0;
    g->recoilBack = 0;
    g->lastHitTimer = 0;
    g->damageFlash = 0;
    g->hitMarkerTimer = 0;
    g->hitMarkerType = 0;
    g->headshotBanner = 0;
    g->headshotStreak = 0;
    g->headshotStreakTimer = 0;
    g->trauma = 0;
    g->boomTimer = 0;
    g->landDip = 0;
    g->moving = false;
    g->moveSpeed = 0;
    g->footTimer = 0.35f;
    g->kills = 0;
    g->score = 0;
    g->shots = 0;
    g->hits = 0;
    g->headshots = 0;
    g->helmetPops = 0;
    g->gameTime = 0;
    g->decalCursor = 0;
    g->dustTimer = 0.12f;

    DamageFX_Init(g->dmgInds, MAX_DAMAGE_INDICATORS, g->dmgNums, MAX_DAMAGE_NUMBERS);
    FX_Init(g->particles, MAX_PARTICLES, g->tracers, MAX_TRACERS, g->decals, MAX_DECALS);
    Net_ResetRound(&g->net);
}

void Game_NewMap(Game* g, WorldMode mode, uint32_t seed) {
    if (g->net.mode == NET_CLIENT) return; // host authority
    if (mode != WORLD_CLASSIC && mode != WORLD_RANDOM && mode != WORLD_RANDOM_256)
        mode = WORLD_CLASSIC;
    World_Generate(&g->world, mode, seed);
    g->worldEpoch++;
    g->nextWorldMode = mode;
    g->set.worldMode = (int)mode;
    g->set.worldSeed = g->world.seed ? g->world.seed : 1u;
    Settings_Save(&g->set, "nga.cfg");
    uint64_t hash = World_LayoutHash(&g->world);
    Net_SetWorld(&g->net, (uint8_t)mode, g->world.seed, g->worldEpoch, hash);
    Game_LoadWorldEntities(g);
    Game_BakeLampShadows(g);
    Game_ResetRoundState(g, 0);
    TraceLog(LOG_INFO, "NEW MAP: mode=%d seed=%u epoch=%u (%d boxes, %d loot) hash=%016llx",
             (int)mode, g->world.seed, g->worldEpoch, g->world.boxCount,
             g->world.pickupCount, (unsigned long long)hash);
}

void Game_ApplyRemoteWorld(Game* g, int mode, uint32_t seed, uint32_t epoch,
                           uint64_t expectedHash) {
    if (g->net.mode != NET_CLIENT) return;
    if (mode < 0 || mode >= WORLD_MODE_COUNT) mode = 0;
    if (g->world.mode == (WorldMode)mode && g->world.seed == seed &&
        g->worldEpoch == epoch &&
        (expectedHash == 0 || World_LayoutHash(&g->world) == expectedHash))
        return;
    World_Generate(&g->world, (WorldMode)mode, seed);
    g->worldEpoch = epoch;
    g->nextWorldMode = (WorldMode)mode;
    g->net.worldMode = (uint8_t)mode;
    g->net.worldSeed = seed;
    g->net.worldEpoch = epoch;
    uint64_t hash = World_LayoutHash(&g->world);
    if (expectedHash != 0 && hash != expectedHash) {
        TraceLog(LOG_WARNING,
                 "REMOTE MAP HASH MISMATCH: mode=%d seed=%u epoch=%u host=%016llx local=%016llx",
                 mode, seed, epoch, (unsigned long long)expectedHash,
                 (unsigned long long)hash);
        Net_Feed(&g->net, "MAP HASH MISMATCH");
    }
    Game_LoadWorldEntities(g);
    Game_BakeLampShadows(g);
    Game_ResetRoundState(g, Game_LocalSpawnIndex(g));
    TraceLog(LOG_INFO, "REMOTE MAP: mode=%d seed=%u epoch=%u (%d boxes, %d loot) hash=%016llx",
             mode, seed, epoch, g->world.boxCount, g->world.pickupCount,
             (unsigned long long)hash);
}

static BoundingBox BarrelBox(const Barrel* b) {
    Vector3 h = { b->radius, 0.65f, b->radius };
    Vector3 c = { b->position.x, 0.65f, b->position.z };
    return (BoundingBox){ Vector3Subtract(c, h), Vector3Add(c, h) };
}

static void ExplodeBarrel(Game* g, int idx, Vector3 hitPos, int credit, bool fromNet) {
    Barrel* b = &g->barrels[idx];
    if (!b->alive) return;
    b->alive = false;
    b->respawnTimer = BARREL_RESPAWN_TIME;
    Vector3 c = { b->position.x, 0.9f, b->position.z };
    FX_SpawnExplosion(g->particles, MAX_PARTICLES, c, 5);
    FX_AddDecal(g->decals, MAX_DECALS, (Vector3){ c.x, 0.10f, c.z }, 1.8f, (Color){ 15, 12, 10, 230 }, &g->decalCursor);
    Audio_PlayExplosion(&g->audio);
    g->trauma = fminf(1.0f, g->trauma + 1.0f);
    // bright blast light for a few frames: lights walls/enemies and is fed
    // into the lit shader as a forced point light (respects lamp shadows)
    g->boomTimer = 0.42f;
    g->boomPos = c;
    (void)hitPos;
    // host (or single-player): enemies live here, damage them and tell clients
    bool clientSide = g->net.active && g->net.mode == NET_CLIENT;
    if (!clientSide) {
        Enemy_AlertAll(g->enemies, MAX_ENEMIES, c, 42.0f); // blast draws attention
        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy* e = &g->enemies[i];
            if (!e->alive) continue;
            float d = Vector3Distance(e->position, c);
            if (d < BARREL_BLAST) {
                float dmg = BARREL_DAMAGE * (1.0f - d / BARREL_BLAST * 0.55f);
                HitResult r = Enemy_ApplyDamage(e, dmg, HIT_TORSO);
                FX_SpawnBlood(g->particles, MAX_PARTICLES, e->position, 8);
                DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, e->position, dmg, r.killed ? 2 : 0);
                if (r.killed) {
                    PlaySoundPitched(g->audio.enemyDie, 0.9f + RandomFloat() * 0.2f);
                    if (!g->net.active || credit == g->net.myId) {
                        g->kills++; g->score += e->elite ? 200 : 100;
                        g->hitMarkerType = 3; g->hitMarkerTimer = HITMARKER_TIME;
                    } else if (g->net.mode == NET_HOST) {
                        // a client shot this barrel: credit their scoreboard
                        Net_NotifyEnemyKilled(&g->net, credit, false, e->elite);
                    }
                }
            }
        }
    }
    // damage player
    float pd = Vector3Distance(g->player.position, c);
    if (pd < BARREL_BLAST) {
        float dmg = BARREL_DAMAGE * 0.60f * (1.0f - pd / BARREL_BLAST * 0.5f);
        if (Player_TakeDamage(&g->player, dmg)) {
            g->damageFlash = 0.6f;
            DamageFX_SpawnIndicator(g->dmgInds, MAX_DAMAGE_INDICATORS, c);
            PlaySoundPitched(g->audio.playerHurt, 0.8f);
        }
    }
    // chain other barrels
    for (int i = 0; i < MAX_BARRELS; i++) {
        if (i == idx || !g->barrels[i].alive) continue;
        Vector3 oc = { g->barrels[i].position.x, 0.9f, g->barrels[i].position.z };
        if (Vector3Distance(oc, c) < BARREL_BLAST * 0.9f) {
            g->barrels[i].health -= BARREL_DAMAGE;
            if (g->barrels[i].health <= 0) ExplodeBarrel(g, i, oc, credit, fromNet);
            else g->barrels[i].flash = 0.2f;
        }
    }
    // host announces the blast so every peer replays it on their own world
    if (!fromNet && g->net.active && g->net.mode == NET_HOST)
        Net_NotifyBarrel(&g->net, idx, c);
}

static void Game_Fire(Game* g, Weapon* w, Vector3 forward) {
    g->fireCd = w->fireRate;
    g->muzzleTimer = MUZZLE_FLASH_TIME;
    w->ammoInMag--;
    g->shots++;
    g->net.pendingShot = true; // clients/host peers see a muzzle event this tick
    Audio_PlayShot(&g->audio, g->curWeapon, 0.96f + RandomFloat() * 0.08f);

    Vector3 dir = ShotDir(g, w, forward);
    Ray ray = { g->camera.position, dir };

    float nearest = 1e9f;
    Vector3 hitPos = { 0 };
    bool didHit = false;
    int hitEnemy = -1;
    int hitBarrel = -1;
    int hitPeer = -1;
    bool hitPeerHead = false;
    bool wallHit = false;

    Vector3 wallP = { 0 };
    float wd = World_RayHitBoxes(ray, g->world.boxes, g->world.boxCount, &wallP);
    if (wd > 0 && wd < nearest) { nearest = wd; hitPos = wallP; didHit = true; hitEnemy = -1; hitBarrel = -1; hitPeer = -1; wallHit = true; }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g->enemies[i].alive) continue;
        BoundingBox bb = Enemy_BodyBox(&g->enemies[i]);
        RayCollision rc = GetRayCollisionBox(ray, bb);
        if (rc.hit && rc.distance < nearest && rc.distance < w->range) {
            nearest = rc.distance; hitPos = rc.point; didHit = true; hitEnemy = i; hitBarrel = -1; hitPeer = -1; wallHit = false;
        }
    }
    // remote players (LAN): body + head boxes, same damage rules as enemies
    if (g->net.active) {
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            const NetPeer* p = &g->net.peers[i];
            if (!p->used || p->id == g->net.myId || !(p->flags & NETF_ALIVE)) continue;
            Vector3 bc = { p->pos.x, p->pos.y + 1.02f, p->pos.z };
            Vector3 bh = { 0.38f, 0.95f, 0.32f };
            RayCollision rc = GetRayCollisionBox(ray, (BoundingBox){ Vector3Subtract(bc, bh), Vector3Add(bc, bh) });
            if (!rc.hit || rc.distance >= nearest || rc.distance >= w->range) continue;
            Vector3 hc = { p->pos.x, p->pos.y + 1.78f, p->pos.z };
            Vector3 hh = { 0.19f, 0.19f, 0.17f };
            RayCollision rh = GetRayCollisionBox(ray, (BoundingBox){ Vector3Subtract(hc, hh), Vector3Add(hc, hh) });
            bool head = rh.hit && rh.distance <= rc.distance + 0.05f;
            nearest = head ? rh.distance : rc.distance;
            hitPos = head ? rh.point : rc.point;
            didHit = true;
            hitEnemy = -1; hitBarrel = -1; hitPeer = p->id; hitPeerHead = head; wallHit = false;
        }
    }
    for (int i = 0; i < MAX_BARRELS; i++) {
        if (!g->barrels[i].alive) continue;
        BoundingBox bb = BarrelBox(&g->barrels[i]);
        RayCollision rc = GetRayCollisionBox(ray, bb);
        if (rc.hit && rc.distance < nearest && rc.distance < w->range) {
            nearest = rc.distance; hitPos = rc.point; didHit = true; hitEnemy = -1; hitBarrel = i; hitPeer = -1; wallHit = false;
        }
    }
    if (!didHit) hitPos = Vector3Add(ray.position, Vector3Scale(ray.direction, 120.0f));

    Vector3 up = { 0, 1, 0 };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, up));
    Vector3 muzzle = Vector3Add(g->camera.position, Vector3Scale(forward, 0.6f));
    muzzle = Vector3Add(muzzle, Vector3Scale(right, 0.12f));
    muzzle.y -= 0.08f;
    Color tc = (g->curWeapon == 2) ? (Color){ 140, 200, 255, 255 } : (Color){ 255, 220, 130, 255 };
    FX_AddTracer(g->tracers, MAX_TRACERS, muzzle, hitPos, tc);
    FX_SpawnMuzzleSmoke(g->particles, MAX_PARTICLES, muzzle, forward, 1);
    FX_SpawnCasing(g->particles, MAX_PARTICLES, muzzle, right);
    g->trauma = fminf(1.0f, g->trauma + (g->curWeapon == 2 ? 0.22f : 0.06f));

    if (didHit) { g->lastHitPos = hitPos; g->lastHitTimer = 0.35f; }

    if (hitPeer >= 0) {
        // PvP hit: victim applies the damage on their own machine
        float dmg = hitPeerHead ? w->headDamage : w->bodyDamage;
        if (g->curWeapon != 2 && nearest > 25.0f) dmg *= ClampF(1.0f - (nearest - 25.0f) * 0.01f, 0.6f, 1.0f);
        g->hits++;
        FX_SpawnBlood(g->particles, MAX_PARTICLES, hitPos, hitPeerHead ? 12 : 6);
        DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, hitPos, dmg, hitPeerHead ? 1 : 0);
        g->hitMarkerType = hitPeerHead ? 2 : 1;
        g->hitMarkerTimer = HITMARKER_TIME;
        Net_SendDamageTo(&g->net, hitPeer, dmg, hitPeerHead, g->player.position, g->net.myId);
    } else if (hitBarrel >= 0) {
        Barrel* b = &g->barrels[hitBarrel];
        float bdmg = (g->curWeapon == 2) ? w->bodyDamage : w->bodyDamage * 0.9f;
        if (g->net.active && g->net.mode == NET_CLIENT) {
            // host owns barrel health: report the hit, local feedback only
            b->flash = 0.15f;
            FX_SpawnSpark(g->particles, MAX_PARTICLES, hitPos, 5);
            Audio_PlayHit(&g->audio, 1.1f);
            Net_SendBarrelHit(&g->net, hitBarrel, bdmg);
            g->hitMarkerType = 1; g->hitMarkerTimer = HITMARKER_TIME;
        } else {
            b->health -= bdmg;
            b->flash = 0.15f;
            FX_SpawnSpark(g->particles, MAX_PARTICLES, hitPos, 5);
            Audio_PlayHit(&g->audio, 1.1f);
            if (b->health <= 0) ExplodeBarrel(g, hitBarrel, hitPos, g->net.myId, false);
            else { g->hitMarkerType = 1; g->hitMarkerTimer = HITMARKER_TIME; }
        }
    } else if (hitEnemy >= 0) {
        Enemy* e = &g->enemies[hitEnemy];
        BoundingBox hb = Enemy_HeadBox(e);
        RayCollision hh = GetRayCollisionBox(ray, hb);
        HitZone zone = Enemy_ClassifyHit(e, hitPos, hh.hit, hh.hit ? hh.distance : 1e9f, nearest);
        float dmg = w->bodyDamage;
        if (zone == HIT_HEAD) dmg = w->headDamage;
        else if (zone == HIT_LEG) dmg = w->bodyDamage * 0.72f;
        else if (zone == HIT_HELMET) dmg = 12.0f; // chip through helmet
        if (g->curWeapon != 2 && nearest > 25.0f) dmg *= ClampF(1.0f - (nearest - 25.0f) * 0.01f, 0.6f, 1.0f);

        if (g->net.active && g->net.mode == NET_CLIENT) {
            // host owns enemy health: report the hit, show local feedback
            g->hits++;
            if (zone == HIT_HELMET) {
                FX_SpawnHelmetPop(g->particles, MAX_PARTICLES, hitPos);
                Audio_PlayHelmetPing(&g->audio);
                DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, hitPos, 0, 0);
            } else if (zone == HIT_HEAD) {
                FX_SpawnHeadBlood(g->particles, MAX_PARTICLES, hitPos, 12);
                Audio_PlayHeadshot(&g->audio);
                DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, hitPos, dmg, 1);
            } else {
                FX_SpawnBlood(g->particles, MAX_PARTICLES, hitPos, zone == HIT_LEG ? 4 : 6);
                Audio_PlayEnemyHurt(&g->audio, 0.9f + RandomFloat() * 0.2f);
                DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, hitPos, dmg, 0);
            }
            g->hitMarkerType = zone == HIT_HEAD ? 2 : 1;
            g->hitMarkerTimer = HITMARKER_TIME;
            Net_SendEnemyHit(&g->net, hitEnemy, (int)zone, dmg, hitPos);
        } else {

        HitResult r = Enemy_ApplyDamage(e, dmg, zone);
        e->alertTimer = 3.5f;
        e->lastSeenPos = g->player.position; // they know where the shot came from
        g->hits++;

        if (zone == HIT_HELMET && r.helmetPopped) {
            g->helmetPops++;
            Vector3 hp = { e->position.x, e->position.y + 0.9f, e->position.z };
            FX_SpawnHelmetPop(g->particles, MAX_PARTICLES, hp);
            FX_AddDecal(g->decals, MAX_DECALS, hitPos, 0.04f, (Color){ 60, 60, 62, 220 }, &g->decalCursor);
            Audio_PlayHelmetPing(&g->audio);
            DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, hitPos, 0, 0);
            // floating text handled via banner lite
            g->hitMarkerType = 1; g->hitMarkerTimer = HITMARKER_TIME;
        } else {
            bool isHead = (zone == HIT_HEAD);
            if (isHead) {
                g->headshots++;
                g->headshotStreak++;
                g->headshotStreakTimer = 4.0f;
                g->headshotBanner = 1.4f;
                FX_SpawnHeadBlood(g->particles, MAX_PARTICLES, hitPos, 12);
                FX_AddDecal(g->decals, MAX_DECALS, hitPos, 0.09f, (Color){ 150, 15, 15, 220 }, &g->decalCursor);
                Audio_PlayHeadshot(&g->audio);
            } else {
                FX_SpawnBlood(g->particles, MAX_PARTICLES, hitPos, zone == HIT_LEG ? 4 : 6);
                if (!r.killed) Audio_PlayEnemyHurt(&g->audio, 0.9f + RandomFloat() * 0.2f);
            }
            bool killed = r.killed;
            int dtype = 0;
            if (killed && isHead) dtype = 2;
            else if (killed) dtype = 2;
            else if (isHead) dtype = 1;
            // show actual damage (helmet pop shows 0 separately)
            DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, hitPos, dmg, dtype);
            if (killed) {
                PlaySoundPitched(g->audio.enemyDie, 0.9f + RandomFloat() * 0.2f);
                FX_SpawnBlood(g->particles, MAX_PARTICLES, e->position, 10);
                FX_AddDecal(g->decals, MAX_DECALS, (Vector3){ e->position.x, 0.10f, e->position.z },
                            0.5f, (Color){ 140, 10, 10, 180 }, &g->decalCursor);
                g->kills++;
                int bonus = isHead ? 150 + g->headshotStreak * 25 : 100;
                if (e->elite) bonus += 100;
                g->score += bonus;
                g->hitMarkerType = isHead ? 4 : 3;
                g->trauma = fminf(1.0f, g->trauma + (isHead ? 0.18f : 0.1f));
                if (g->net.active && g->net.mode == NET_HOST)
                    Net_NotifyEnemyKilled(&g->net, g->net.myId, isHead, e->elite);
            } else {
                g->hitMarkerType = isHead ? 2 : (zone == HIT_LEG ? 1 : 1);
            }
            g->hitMarkerTimer = HITMARKER_TIME;
        }
        }
    } else if (wallHit) {
        Audio_PlayHit(&g->audio, 0.9f + RandomFloat() * 0.2f);
        FX_SpawnSpark(g->particles, MAX_PARTICLES, hitPos, 7);
        FX_SpawnSmoke(g->particles, MAX_PARTICLES, hitPos, 1);
        FX_AddDecal(g->decals, MAX_DECALS, hitPos, 0.05f, (Color){ 20, 15, 10, 200 }, &g->decalCursor);
    } else {
        FX_SpawnSmoke(g->particles, MAX_PARTICLES, muzzle, 1);
    }

    g->pitch += w->recoilPitch * RandRange(0.9f, 1.3f);
    g->yaw += (RandomFloat() - 0.5f) * w->recoilYaw;
    g->recoilBack += w->recoilBack;
    if (g->net.active && g->net.mode == NET_CLIENT) {
        // let the host's AI hear the shot (enemies are simulated there)
        Net_SendNoise(&g->net, g->player.position);
    } else {
        Enemy_AlertAll(g->enemies, MAX_ENEMIES, g->player.position, 42.0f);
    }
}

static void Game_StartReload(Game* g, Weapon* w) {
    g->reloading = true;
    g->reloadEmpty = (w->ammoInMag == 0);
    g->reloadTotal = Weapon_ReloadTime(w, g->reloadEmpty);
    g->reloadTimer = g->reloadTotal;
    g->reloadPrevT = 0;
    g->reloadPlayedInsert = false;
    PlaySoundPitched(g->audio.reload, 0.95f + RandomFloat() * 0.1f);
}

// Apply LAN events queued by net.c (all side effects live in game code).
static void Game_ApplyNetEvents(Game* g) {
    for (int ei = 0; ei < g->net.eventCount; ei++) {
        NetEvent* e = &g->net.events[ei];
        switch (e->type) {
        case NEV_DAMAGE: {
            if (g->net.localDead) break;
            if (Player_TakeDamage(&g->player, e->f0)) {
                g->damageFlash = fmaxf(g->damageFlash, 0.45f);
                DamageFX_SpawnIndicator(g->dmgInds, MAX_DAMAGE_INDICATORS, e->a);
                if (SoundLoaded(g->audio.playerHurt))
                    PlaySoundPitched(g->audio.playerHurt, 0.95f + RandomFloat() * 0.15f);
                g->trauma = fminf(1.0f, g->trauma + 0.2f);
                g->net.lastAttackerId = e->i1;
                g->net.lastAttackerTime = 5.0f;
                if (e->i1 <= 0 && g->net.mode == NET_CLIENT) {
                    // enemy damage: mirror the host sim's melee shove locally
                    // (the victim is dragged a little toward the striker)
                    Vector3 toE = Vector3Subtract(e->a, g->player.position);
                    toE.y = 0;
                    float sd = Vector3Length(toE);
                    if (sd > 0.01f && sd < ENEMY_MELEE_RANGE + 1.7f) {
                        Vector3 shove = Vector3Scale(toE, 1.4f * 0.18f / sd);
                        g->player.position.x = ClampF(g->player.position.x + shove.x, -MAP_LIMIT, MAP_LIMIT);
                        g->player.position.z = ClampF(g->player.position.z + shove.z, -MAP_LIMIT, MAP_LIMIT);
                    }
                }
            }
            break;
        }
        case NEV_ENEMY_HIT: {
            if (e->i0 < 0 || e->i0 >= MAX_ENEMIES) break;
            Enemy* en = &g->enemies[e->i0];
            if (!en->alive) break;
            HitZone zone = (e->i1 == HIT_HEAD) ? HIT_HEAD
                         : (e->i1 == HIT_LEG) ? HIT_LEG
                         : (e->i1 == HIT_HELMET) ? HIT_HELMET : HIT_TORSO;
            HitResult r = Enemy_ApplyDamage(en, e->f0, zone);
            FX_SpawnBlood(g->particles, MAX_PARTICLES, e->a, 6);
            DamageFX_SpawnNumber(g->dmgNums, MAX_DAMAGE_NUMBERS, e->a, e->f0,
                                 r.killed ? 2 : (zone == HIT_HEAD ? 1 : 0));
            if (r.killed) {
                PlaySoundPitched(g->audio.enemyDie, 0.9f + RandomFloat() * 0.2f);
                FX_SpawnBlood(g->particles, MAX_PARTICLES, en->position, 10);
                FX_AddDecal(g->decals, MAX_DECALS, (Vector3){ en->position.x, 0.10f, en->position.z },
                            0.5f, (Color){ 140, 10, 10, 180 }, &g->decalCursor);
                Net_NotifyEnemyKilled(&g->net, e->i2, zone == HIT_HEAD, en->elite);
            }
            break;
        }
        case NEV_BARREL_HIT: {
            if (e->i0 < 0 || e->i0 >= MAX_BARRELS) break;
            Barrel* b = &g->barrels[e->i0];
            if (!b->alive) break;
            b->health -= e->f0;
            if (b->health <= 0) ExplodeBarrel(g, e->i0, (Vector3){ b->position.x, 0.9f, b->position.z }, e->i2, false);
            else b->flash = 0.2f;
            break;
        }
        case NEV_BARREL: {
            if (e->i0 < 0 || e->i0 >= MAX_BARRELS) break;
            ExplodeBarrel(g, e->i0, e->a, 0, true);
            break;
        }
        case NEV_REMOTE_SHOT: {
            FX_AddTracer(g->tracers, MAX_TRACERS, e->a, e->b, (Color){ 255, 214, 130, 190 });
            if (e->i0 >= 0 && e->i0 < WEAPON_COUNT && SoundLoaded(g->audio.shotSounds[e->i0]))
                PlaySoundPitched(g->audio.shotSounds[e->i0], 0.92f + RandomFloat() * 0.12f);
            break;
        }
        case NEV_ENEMY_SHOT: {
            if (SoundLoaded(g->audio.shotAKR))
                PlaySoundPitched(g->audio.shotAKR, 1.05f + RandomFloat() * 0.1f);
            break;
        }
        case NEV_ENEMY_DIE: {
            PlaySoundPitched(g->audio.enemyDie, 0.9f + RandomFloat() * 0.2f);
            FX_SpawnBlood(g->particles, MAX_PARTICLES, e->a, 10);
            break;
        }
        case NEV_NOISE: {
            Enemy_AlertAll(g->enemies, MAX_ENEMIES, e->a, e->f0 > 1.0f ? e->f0 : 34.0f);
            break;
        }
        case NEV_MY_KILL: {
            bool head = e->i0 != 0;
            bool elite = e->i1 != 0;
            g->kills++;
            if (head) {
                // score this kill's headshot like the shooting host does:
                // count it into the streak first, then pay out the bonus
                g->headshots++;
                g->headshotStreak++;
                g->headshotStreakTimer = 4.0f;
                g->headshotBanner = 1.4f;
            }
            g->score += (head ? 150 + g->headshotStreak * 25 : 100) + (elite ? 100 : 0);
            g->hitMarkerType = head ? 4 : 3;
            g->hitMarkerTimer = HITMARKER_TIME;
            PlaySoundPitched(g->audio.enemyDie, 0.9f + RandomFloat() * 0.2f);
            break;
        }
        default: break;
        }
    }
    g->net.eventCount = 0;
}

void Game_Update(Game* g, float dt) {
    if (dt > 0.05f) dt = 0.05f;
    if (dt < 0.00001f) dt = 0.00001f;

    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
        g->set.fullscreen = IsWindowFullscreen();
        Settings_Save(&g->set, "nga.cfg");
    }
    if (IsKeyPressed(KEY_F6) && g->shadow.available) g->shadow.enabled = !g->shadow.enabled;
    // ESC opens the settings menu. Single-player freezes the world behind it;
    // multiplayer keeps the net pump, host AI and snapshots running, so only
    // local gameplay input is suppressed while the overlay is up.
    if (IsKeyPressed(KEY_ESCAPE)) {
        g->paused = !g->paused;
        TraceLog(LOG_INFO, "menu %s (%s)", g->paused ? "opened" : "closed",
                 g->net.active ? "multiplayer: world keeps running" : "single-player: world paused");
        if (g->paused) { EnableCursor(); SetMasterVolume(g->masterVol * 0.25f); }
        else { DisableCursor(); SetMasterVolume(g->masterVol); }
    }
    if (g->paused && !g->net.active) return; // SP: settings menu freezes the world

    bool menuOpen = g->paused; // MP only (SP returned above): keep the world live


    g->gameTime += dt;
    g->trauma = fmaxf(0, g->trauma - dt * 1.6f);
    if (g->headshotBanner > 0) g->headshotBanner -= dt;
    if (g->headshotStreakTimer > 0) { g->headshotStreakTimer -= dt; if (g->headshotStreakTimer <= 0) g->headshotStreak = 0; }
    Weapon* w = &g->weapons[g->curWeapon];
    bool mpDead = g->net.active && g->net.localDead;

    if (!mpDead && !menuOpen) {
    int keys[WEAPON_COUNT] = { KEY_ONE, KEY_TWO, KEY_THREE };
    for (int k = 0; k < WEAPON_COUNT; k++) {
        if (IsKeyPressed(keys[k]) && g->curWeapon != k) {
            g->curWeapon = k; w = &g->weapons[k];
            g->reloading = false; g->reloadTimer = 0;
            g->fireCd = 0.35f; g->switchTimer = 0.35f;
            break;
        }
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        int n = (g->curWeapon + (wheel > 0 ? 2 : 1)) % WEAPON_COUNT;
        g->curWeapon = n; w = &g->weapons[n];
        g->reloading = false; g->reloadTimer = 0;
        g->fireCd = 0.35f; g->switchTimer = 0.35f;
    }
    }

    bool sprintHeld = !menuOpen && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    g->adsActive = !mpDead && !menuOpen && IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !sprintHeld && g->sprintBlend < 0.5f;
    float tblend = g->adsActive ? 1.0f : 0.0f;
    g->adsBlend += (tblend - g->adsBlend) * (1.0f - expf(-18.0f * dt));
    float baseFov = (float)g->set.fov;
    g->camera.fovy = baseFov + (w->adsFov - baseFov) * g->adsBlend + 4.0f * g->sprintBlend;

    float sensMult = 1.0f + (w->adsSens - 1.0f) * g->adsBlend;
    Vector2 md = menuOpen ? (Vector2){ 0, 0 } : GetMouseDelta();
    g->yaw -= md.x * g->sens * sensMult;
    g->pitch -= md.y * g->sens * sensMult;
    g->pitch = ClampF(g->pitch, -1.5f, 1.5f);

    if (!mpDead && !menuOpen && IsKeyPressed(KEY_SPACE) && g->player.grounded) {
        g->player.velocity.y = JUMP_SPEED;
        g->player.grounded = false;
        PlaySoundPitched(g->audio.jump, 0.95f + RandomFloat() * 0.1f);
        FX_SpawnDirtKick(g->particles, MAX_PARTICLES, g->player.position, 4);
    }

    bool wasGrounded = g->player.grounded;
    float fallVy = g->player.velocity.y;
    bool sprintWant = sprintHeld && g->switchTimer <= 0.0f && !mpDead;
    if (mpDead) {
        g->moving = false;
        g->moveSpeed = 0.0f;
        g->player.velocity = (Vector3){ 0, 0, 0 };
    } else {
        Player_Update(&g->player, g->world.boxes, g->world.boxCount, g->world.limit, g->yaw, dt,
                      g->adsActive, sprintWant, !menuOpen, &g->bob, &g->moving, &g->bobTime, &g->moveSpeed);
    }
    float sprintTarget = (g->moveSpeed > 9.2f && g->player.grounded) ? 1.0f : 0.0f;
    g->sprintBlend += (sprintTarget - g->sprintBlend) * (1.0f - expf(-9.0f * dt));
    // sprinting cancels a reload, like AAA shooters
    if (g->sprintBlend > 0.4f && g->reloading) { g->reloading = false; g->reloadTimer = 0; }
    // landing dip: viewmodel kicks down on hard landings
    if (!wasGrounded && g->player.grounded && fallVy < -8.0f)
        g->landDip = ClampF(-fallVy * 0.012f, 0.04f, 0.15f);
    g->landDip = fmaxf(0.0f, g->landDip - dt * 0.5f);

    if (g->moving && g->player.grounded) {
        float interval = ClampF(0.42f * (PLAYER_WALK_SPEED / fmaxf(g->moveSpeed, 5.0f)), 0.26f, 0.42f);
        g->footTimer += dt;
        if (g->footTimer >= interval) {
            g->footTimer = 0;
            PlaySoundPitched(g->audio.footstep, 0.94f + RandomFloat() * 0.16f - g->sprintBlend * 0.10f);
            FX_SpawnDirtKick(g->particles, MAX_PARTICLES, g->player.position, 1 + (g->sprintBlend > 0.5f ? 1 : 0));
            // sprinting is loud: close enemies hear the footsteps
            if (g->sprintBlend > 0.5f)
                Enemy_AlertAll(g->enemies, MAX_ENEMIES, g->player.position, 13.0f);
        }
    } else g->footTimer = 0.35f;

    unsigned pmaskBefore = 0;
    for (int i = 0; i < g->world.pickupCount; i++) if (g->pickups[i].active) pmaskBefore |= 1u << i;
    if (!mpDead)
        Pickup_Update(g->pickups, g->world.pickupCount, &g->player, g->weapons, WEAPON_COUNT,
                      &g->audio, dt, g->particles, MAX_PARTICLES);
    if (g->net.active && g->net.mode == NET_CLIENT) {
        // locally consumed a pickup? ask the host to consume it for everyone
        for (int i = 0; i < g->world.pickupCount; i++) {
            if ((pmaskBefore >> i) & 1u) {
                if (!g->pickups[i].active) {
                    Net_RequestPickup(&g->net, i);
                    Net_NoteLocalPickup(&g->net, i);
                }
            }
        }
    }

    // barrels respawn + flash decay (respawn is host/single-player authority;
    // clients mirror the host's barrel mask instead)
    for (int i = 0; i < MAX_BARRELS; i++) {
        if (g->barrels[i].flash > 0) g->barrels[i].flash -= dt;
        if (!g->barrels[i].alive && !(g->net.active && g->net.mode == NET_CLIENT)) {
            g->barrels[i].respawnTimer -= dt;
            if (g->barrels[i].respawnTimer <= 0) {
                g->barrels[i].alive = true;
                g->barrels[i].health = BARREL_HEALTH;
            }
        }
    }

    // ambient dust motes
    g->dustTimer -= dt;
    if (g->dustTimer <= 0) {
        g->dustTimer = 0.12f;
        FX_SpawnDustMote(g->particles, MAX_PARTICLES, g->player.position, 14.0f);
    }

    g->camera.position = PlayerEyePos(&g->player);
    // camera shake: smooth time-based sway (white-noise per frame at 200+ fps
    // looks like whole-scene flicker, so use layered sines instead)
    if (g->trauma > 0.01f) {
        float s = g->trauma * g->trauma * 0.22f;
        float t = g->gameTime * 61.0f;
        g->camera.position.x += sinf(t * 1.13f) * s * 0.5f + sinf(t * 2.71f) * s * 0.22f;
        g->camera.position.y += cosf(t * 1.71f) * s * 0.5f + sinf(t * 3.17f) * s * 0.22f;
    }
    Vector3 forward = { cosf(g->pitch) * sinf(g->yaw), sinf(g->pitch), cosf(g->pitch) * cosf(g->yaw) };
    g->camera.target = Vector3Add(g->camera.position, forward);

    g->fireCd -= dt; g->muzzleTimer -= dt; g->lastHitTimer -= dt;
    if (g->boomTimer > 0) g->boomTimer -= dt;
    g->damageFlash -= dt; g->hitMarkerTimer -= dt; g->switchTimer -= dt;
    DamageFX_Update(g->dmgInds, g->dmgNums, MAX_DAMAGE_INDICATORS, MAX_DAMAGE_NUMBERS, dt);
    FX_Update(g->particles, MAX_PARTICLES, g->tracers, MAX_TRACERS, g->decals, MAX_DECALS, dt);

    if (g->reloading) {
        float prevT = g->reloadPrevT;
        g->reloadTimer -= dt;
        float newT = 1.0f - (g->reloadTimer / g->reloadTotal);
        if (newT > 1) newT = 1;
        g->reloadPrevT = newT;
        if (!g->reloadPlayedInsert && Weapon_ReloadInsertCrossed(prevT, newT)) {
            g->reloadPlayedInsert = true;
            Audio_PlayReloadClick(&g->audio, 1.35f);
        }
        if (g->reloadTimer <= 0) {
            int need = w->magSize - w->ammoInMag;
            int give = (need < w->ammoReserve) ? need : w->ammoReserve;
            w->ammoInMag += give; w->ammoReserve -= give;
            g->reloading = false;
            Audio_PlayReloadClick(&g->audio, 1.1f);
        }
    }
    if (!mpDead && !menuOpen && IsKeyPressed(KEY_R) && !g->reloading && w->ammoInMag < w->magSize && w->ammoReserve > 0) {
        Game_StartReload(g, w);
    }
    if (!mpDead && !menuOpen && w->ammoInMag == 0 && w->ammoReserve > 0 && !g->reloading) {
        if ((w->autoFire && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Game_StartReload(g, w);
        }
    }

    bool wantFire = !mpDead && !menuOpen && (w->autoFire ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    if (wantFire && g->fireCd <= 0 && !g->reloading && w->ammoInMag > 0 &&
        g->switchTimer <= 0 && g->sprintBlend < 0.5f)
        Game_Fire(g, w, forward);
    if (wantFire && w->ammoInMag == 0 && w->ammoReserve == 0 && g->fireCd <= 0) {
        g->fireCd = 0.3f;
        PlaySoundPitched(g->audio.reload, 1.6f);
    }

    float decay = 1.0f - expf(-14.0f * dt);
    g->recoilBack = Lerp(g->recoilBack, 0.0f, decay);
    Vector2 tgt = { -md.x * 0.0016f, -md.y * 0.0016f };
    g->sway.x += (tgt.x - g->sway.x) * (1.0f - expf(-10.0f * dt));
    g->sway.y += (tgt.y - g->sway.y) * (1.0f - expf(-10.0f * dt));

    // Enemy AI: single-player + host simulate it; clients mirror snapshots.
    // Each enemy picks its own target every frame: the local player plus one
    // stub Player per living remote peer (fed from their net state). With
    // infighting on, one stub per living bot also competes as a target; bot
    // stubs take damage from the AI, then that damage is applied to the real
    // enemy after the update. Clients see the results through snapshots.
    bool enemySim = (!g->net.active || g->net.mode == NET_HOST) && g->set.aiEnabled;
    if (enemySim) {
        enum { MAX_TARGETS = NET_MAX_PLAYERS + MAX_ENEMIES };
        EnemyTarget tgts[MAX_TARGETS];
        Player stubs[MAX_TARGETS];
        float beforeHp[MAX_TARGETS];
        int tgtIds[MAX_TARGETS];
        int nt = 0;
        tgts[nt].player = &g->player;
        tgts[nt].eye = PlayerEyePos(&g->player);
        tgts[nt].enemyId = -1;
        tgts[nt].moving = g->moving;
        tgts[nt].airborne = !g->player.grounded;
        tgts[nt].isLocal = true;
        tgtIds[nt] = -1;
        nt++;
        if (g->net.active && g->net.mode == NET_HOST) {
            for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                const NetPeer* p = &g->net.peers[i];
                if (!p->used || p->id == g->net.myId) continue;
                if (!(p->flags & NETF_ALIVE)) continue;
                Player* st = &stubs[nt];
                *st = g->player;
                st->position = p->targetPos;
                st->velocity = (Vector3){ 0, 0, 0 };
                st->grounded = true;
                st->health = p->health;
                st->spawnProtect = 0;
                tgts[nt].player = st;
                tgts[nt].eye = PlayerEyePos(st);
                tgts[nt].enemyId = -1;
                tgts[nt].moving = (p->flags & NETF_MOVING) != 0;
                tgts[nt].airborne = false;
                tgts[nt].isLocal = false;
                tgtIds[nt] = p->id;
                nt++;
            }
        }
        if (g->set.infighting) {
            for (int i = 0; i < MAX_ENEMIES; i++) {
                const Enemy* e = &g->enemies[i];
                if (!e->alive) continue;
                Player* st = &stubs[nt];
                *st = g->player; // baseline fields, then override
                // Player.position is at the feet; enemy.position is body center.
                st->position = (Vector3){ e->position.x, e->position.y - 0.9f, e->position.z };
                st->velocity = (Vector3){ 0, 0, 0 };
                st->grounded = true;
                st->health = e->health;
                st->spawnProtect = 0; // bots must not inherit spawn protection
                tgts[nt].player = st;
                tgts[nt].eye = EnemyEyePos(e);
                tgts[nt].enemyId = i;
                // Bots strafe/advance constantly; treat their target as moving
                // so infighting does not become laser-accurate.
                tgts[nt].moving = true;
                tgts[nt].airborne = false;
                tgts[nt].isLocal = false;
                tgtIds[nt] = -1;
                nt++;
            }
        }
        for (int k = 0; k < nt; k++) beforeHp[k] = tgts[k].player->health;
        Enemy_UpdateAll(g->enemies, MAX_ENEMIES, tgts, nt,
                        g->world.boxes, g->world.boxCount, &g->world,
                        g->world.limit, g->world.enemySpawns,
                        g->world.enemySpawnCount, &g->audio, dt,
                        g->dmgInds, MAX_DAMAGE_INDICATORS,
                        &g->damageFlash, g->gameTime);
        for (int k = 0; k < nt; k++) {
            float dealt = beforeHp[k] - tgts[k].player->health;
            if (dealt <= 0.01f) continue;
            int eidx = tgts[k].enemyId;
            if (eidx >= 0) {
                // The AI shot a bot stub. Translate that health loss into real
                // enemy damage so health, death, respawn and snapshots stay in
                // one authoritative place.
                Enemy* victim = &g->enemies[eidx];
                if (!victim->alive) continue;
                HitResult r = Enemy_ApplyDamage(victim, dealt, HIT_TORSO);
                FX_SpawnBlood(g->particles, MAX_PARTICLES, victim->position,
                              r.killed ? 10 : 4);
                if (r.killed) {
                    PlaySoundPitched(g->audio.enemyDie, 0.9f + RandomFloat() * 0.2f);
                    FX_AddDecal(g->decals, MAX_DECALS,
                                (Vector3){ victim->position.x, 0.10f, victim->position.z },
                                0.5f, (Color){ 140, 10, 10, 180 }, &g->decalCursor);
                    TraceLog(LOG_INFO, "AI infighting: bot %d was killed by another bot", eidx);
                }
                continue;
            }
            if (tgtIds[k] > 0) {
                // nearby strike direction for the victim's damage indicator
                Vector3 ap = tgts[k].player->position;
                float bd = 1e30f;
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!g->enemies[i].alive) continue;
                    float d = Vector3DistanceSqr(g->enemies[i].position, tgts[k].player->position);
                    if (d < bd) { bd = d; ap = g->enemies[i].position; }
                }
                Net_SendDamageTo(&g->net, tgtIds[k], dealt, false, ap, 0);
            }
        }
    }

    // death
    if (g->player.health <= 0) {
        if (g->net.active) {
            if (!g->net.localDead) {
                g->net.localDead = true;
                if (g->player.health < 0) g->player.health = 0; // keep the HUD sane
                g->net.respawnTimer = 3.0f;
                g->net.selfDeaths++;
                g->reloading = false;
                g->damageFlash = 1.0f;
                g->trauma = fminf(1.0f, g->trauma + 0.6f);
                int killer = (g->net.lastAttackerTime > 0) ? g->net.lastAttackerId : 0;
                Net_SendKill(&g->net, killer, 0, false);
            }
        } else {
            Player_Respawn(&g->player);
            for (int k = 0; k < WEAPON_COUNT; k++) {
                g->weapons[k].ammoInMag = g->weapons[k].magSize;
                g->weapons[k].ammoReserve = g->weapons[k].reserveMax;
            }
            g->reloading = false;
            g->damageFlash = 0.6f;
            g->headshotStreak = 0;
        }
    }
    if (g->net.active && g->net.localDead) {
        g->net.respawnTimer -= dt;
        if (g->net.respawnTimer <= 0) {
            // first spawn point not occupied by a living remote (falls back to
            // the round-robin pick when everyone is clustered)
            int sp = (g->net.myId - 1 + g->net.selfDeaths) % NET_MAX_PLAYERS;
            for (int k = 0; k < NET_MAX_PLAYERS; k++) {
                int c = (sp + k) % NET_MAX_PLAYERS;
                Vector3 cp = Net_SpawnPos(c);
                bool taken = false;
                for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                    const NetPeer* pr = &g->net.peers[i];
                    if (!pr->used || pr->id == g->net.myId) continue;
                    if ((pr->flags & NETF_ALIVE) &&
                        Vector3Distance(pr->targetPos, cp) < 2.0f) { taken = true; break; }
                }
                if (!taken) { sp = c; break; }
            }
            Vector3 rp = Net_SpawnPos(sp);
            g->player.position = rp;
            g->player.velocity = (Vector3){ 0, 0, 0 };
            g->player.health = (float)PLAYER_MAX_HEALTH;
            g->player.spawnProtect = 2.0f;
            g->player.grounded = true;
            g->yaw = Net_SpawnYaw(sp);
            g->pitch = 0;
            for (int k = 0; k < WEAPON_COUNT; k++) {
                g->weapons[k].ammoInMag = g->weapons[k].magSize;
                g->weapons[k].ammoReserve = g->weapons[k].reserveMax;
            }
            g->net.localDead = false;
            g->net.lastAttackerId = 0;
            g->net.lastAttackerTime = 0;
        }
    }

    // ---- LAN: pump sockets + apply queued events ----
    if (g->net.active) {
        NetLocalState ls;
        ls.pos = g->player.position;
        ls.yaw = g->yaw;
        ls.pitch = g->pitch;
        ls.health = g->player.health;
        ls.weapon = g->curWeapon;
        ls.flags = 0;
        if (!g->net.localDead && g->player.health > 0) ls.flags |= NETF_ALIVE;
        if (g->net.pendingShot) ls.flags |= NETF_FIRING;
        if (g->sprintBlend > 0.5f) ls.flags |= NETF_SPRINT;
        if (g->adsActive) ls.flags |= NETF_ADS;
        if (g->moving) ls.flags |= NETF_MOVING;
        ls.kills = g->kills;
        ls.deaths = g->net.selfDeaths;
        Net_Update(&g->net, dt, &ls, g->enemies, MAX_ENEMIES,
                   g->pickups, g->world.pickupCount, g->barrels, MAX_BARRELS);
        if (g->net.mode == NET_CLIENT && g->net.worldChanged) {
            g->net.worldChanged = false;
            Game_ApplyRemoteWorld(g, g->net.worldMode, g->net.worldSeed,
                                  g->net.worldEpoch, g->net.worldHash);
            g->net.spawnApplied = true;
            g->net.pendingSpawn = Game_LocalSpawnIndex(g);
        }
        Game_ApplyNetEvents(g);
        // client spawn handshake: host assigned our spawn point
        if (g->net.mode == NET_CLIENT && g->net.connected && !g->net.spawnApplied) {
            int sp = g->net.pendingSpawn;
            Vector3 sp3 = Net_SpawnPos(sp);
            g->player.position = sp3;
            g->player.velocity = (Vector3){ 0, 0, 0 };
            g->player.health = (float)PLAYER_MAX_HEALTH;
            g->player.spawnProtect = 2.5f;
            g->player.grounded = true;
            g->yaw = Net_SpawnYaw(sp);
            g->pitch = 0;
            g->net.spawnApplied = true;
        }
    }
}

static void DrawBarrels(const Barrel* barrels, int n, float time, bool fakeBlobs) {
    for (int i = 0; i < n; i++) {
        const Barrel* b = &barrels[i];
        if (!b->alive) continue;
        Vector3 p = { b->position.x, 0, b->position.z };
        if (fakeBlobs) {
            rlDrawRenderBatchActive();
            rlDisableDepthMask();
            DrawCylinder((Vector3){ p.x, 0.14f, p.z }, 0.65f, 0.65f, 0.02f, 12, (Color){ 0, 0, 0, 60 });
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
        }
        bool hot = b->flash > 0;
        Color body = hot ? WHITE : (Color){ 170, 40, 30, 255 };
        Color dark = (Color){ 90, 20, 16, 255 };
        Color band = (Color){ 230, 200, 60, 255 };
        DrawCylinderEx((Vector3){ p.x, 0.06f, p.z }, (Vector3){ p.x, 0.10f, p.z }, 0.62f, 0.65f, 12, (Color){ 60, 55, 50, 255 });
        DrawCylinder((Vector3){ p.x, 0.75f, p.z }, b->radius, b->radius, 1.3f, 14, body);
        DrawCube((Vector3){ p.x, 1.42f, p.z }, b->radius * 1.7f, 0.06f, b->radius * 1.7f, dark); // lid
        DrawCylinder((Vector3){ p.x, 1.05f, p.z }, b->radius * 1.01f, b->radius * 1.01f, 0.12f, 14, band);
        DrawCylinder((Vector3){ p.x, 0.45f, p.z }, b->radius * 1.01f, b->radius * 1.01f, 0.12f, 14, band);
        // hazard diamonds (offset outward to avoid coplanar flicker)
        DrawCube((Vector3){ p.x, 0.75f, p.z + b->radius + 0.02f }, 0.22f, 0.22f, 0.02f, (Color){ 240, 200, 60, 255 });
        DrawCube((Vector3){ p.x, 0.75f, p.z - b->radius - 0.02f }, 0.22f, 0.22f, 0.02f, (Color){ 240, 200, 60, 255 });
        // opaque status lamp (no transparent halo = no sorting pop)
        float pulse = 0.5f + 0.5f * sinf(time * 3.0f + (float)i * 1.7f);
        Color lamp = (b->health < BARREL_HEALTH * 0.5f) ? (Color){ 255, 60, 30, 255 } : (Color){ 255, 160, 60, 255 };
        DrawSphereEx((Vector3){ p.x, 1.50f, p.z }, 0.06f + pulse * 0.015f, 8, 6, lamp);
    }
}

// Casters for the static lamp atlas: world geometry only. Barrels/actors
// are skipped so a lamp never shades itself through its own fixture/body
// (a lamp sitting inside a barrel lid would black out its whole pool).
static void CastLampCasters(void* user) {
    Game* g = (Game*)user;
    World_DrawBoxes(&g->world);
}

static void Game_BakeLampShadows(Game* g) {
    if (!g->shadow.available) return;
    int n = 0;
    for (int i = 0; i < g->world.barrelCount && n < SHADOW_MAX_BAKED_LAMPS; i++) {
        if (!g->barrels[i].alive) continue;
        g->lampPos[n++] = (Vector3){ g->barrels[i].position.x, 1.5f, g->barrels[i].position.z };
    }
    for (int i = 0; i < g->world.streetLampCount && n < SHADOW_MAX_BAKED_LAMPS; i++)
        g->lampPos[n++] = g->world.streetLamps[i];
    for (int i = 0; i < g->world.accentLampCount && n < SHADOW_MAX_BAKED_LAMPS; i++)
        g->lampPos[n++] = g->world.accentLamps[i];
    g->lampCount = n;
    Shadow_BakeLamps(&g->shadow, g->lampPos, n, CastLampCasters, g);
    // Match the atmosphere to the arena size. The classic 42/65 fog would
    // erase a 256x256 map; the big arena gets a much longer falloff.
    Shadow_SetFog(&g->shadow, g->world.limit > 80.0f ? 105.0f : 42.0f,
                  g->world.limit > 80.0f ? 140.0f : 65.0f,
                  g->world.limit > 80.0f ? 0.62f : 0.78f);
}

// Dynamic casters for the per-frame lamp atlas: moving bodies only, drawn as
// cheap box silhouettes -- each face is 128px, so boxes read the same as the
// full models there while costing ~10x fewer draw calls. The static world is
// already in the baked atlas and barrels must stay out of both so a lamp can
// never shade its own fixture/pool.
static void CastLampDynActors(void* user) {
    Game* g = (Game*)user;
    Vector3 pp = g->player.position;
    DrawCubeV((Vector3){ pp.x, pp.y + 0.50f, pp.z }, (Vector3){ 0.58f, 1.00f, 0.44f }, WHITE);
    DrawCubeV((Vector3){ pp.x, pp.y + 1.45f, pp.z }, (Vector3){ 0.72f, 0.92f, 0.52f }, WHITE);
    DrawCubeV((Vector3){ pp.x, pp.y + 2.06f, pp.z }, (Vector3){ 0.32f, 0.32f, 0.32f }, WHITE);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy* e = &g->enemies[i];
        if (!e->alive) continue;
        DrawCubeV((Vector3){ e->position.x, e->position.y + 0.85f, e->position.z },
                  (Vector3){ 0.68f, 1.70f, 0.55f }, WHITE);
    }
    Net_DrawCasterBoxes(&g->net); // remote players
}

// Collect up to 8 colored point lights for the lit shader. Muzzle flashes
// always make the cut (gameplay readability); everything else competes by
// distance so the 8 slots go to what the camera can actually see. Baked
// static lights carry a lampIndex so the shader can sample their shadows.
static void GatherLights(Game* g, PointLight* out, int* count) {
    if (!g->set.pointLights) { *count = 0; return; }
    Vector3 camPos = g->camera.position;

    struct { PointLight l; float d2; } cand[16];
    int c = 0;

    if (g->muzzleTimer > 0 && c < 16) {
        Vector3 fwd = Vector3Normalize(Vector3Subtract(g->camera.target, g->camera.position));
        Vector3 p = Vector3Add(camPos, Vector3Scale(fwd, 1.0f));
        p.y -= 0.15f;
        cand[c].l.pos = p;
        cand[c].l.color = (Vector3){ 1.55f, 1.15f, 0.62f };
        cand[c].l.radius = 7.5f;
        cand[c].l.lampIndex = -1;
        cand[c].d2 = -1.0f; // always first
        c++;
    }
    if (g->boomTimer > 0 && c < 16) {
        // barrel explosion blast: bright, warm, wide - second only to muzzle
        cand[c].l.pos = g->boomPos;
        cand[c].l.color = (Vector3){ 2.1f, 1.15f, 0.45f };
        cand[c].l.radius = 14.0f;
        cand[c].l.lampIndex = -1;
        cand[c].d2 = -0.5f;
        c++;
    }
    for (int i = 0; i < MAX_ENEMIES && c < 16; i++) {
        const Enemy* e = &g->enemies[i];
        if (!e->alive || e->muzzleFlash <= 0) continue;
        Vector3 d = { sinf(e->facing), 0, cosf(e->facing) };
        cand[c].l.pos = (Vector3){ e->position.x + d.x * 1.2f, e->position.y + 0.15f, e->position.z + d.z * 1.2f };
        cand[c].l.color = (Vector3){ 1.45f, 1.05f, 0.58f };
        cand[c].l.radius = 7.0f;
        cand[c].l.lampIndex = -1;
        Vector3 dd = Vector3Subtract(cand[c].l.pos, camPos);
        cand[c].d2 = dd.x * dd.x + dd.y * dd.y + dd.z * dd.z;
        c++;
    }
    for (int i = 0; i < MAX_BARRELS && c < 16; i++) {
        const Barrel* b = &g->barrels[i];
        if (!b->alive) continue;
        float dx = b->position.x - camPos.x, dz = b->position.z - camPos.z;
        if (dx * dx + dz * dz > 40.0f * 40.0f) continue;
        cand[c].l.pos = (Vector3){ b->position.x, 1.5f, b->position.z };
        cand[c].l.color = (b->health < BARREL_HEALTH * 0.5f)
                        ? (Vector3){ 1.25f, 0.33f, 0.16f }
                        : (Vector3){ 1.05f, 0.68f, 0.27f };
        cand[c].l.radius = 3.6f;
        cand[c].l.lampIndex = i; // baked atlas order matches barrel order
        cand[c].d2 = dx * dx + dz * dz;
        c++;
    }
    int streetBase = g->world.barrelCount;
    int accentBase = streetBase + g->world.streetLampCount;
    for (int i = 0; i < g->world.streetLampCount && c < 16; i++) {
        Vector3 lp = g->world.streetLamps[i];
        float dx = lp.x - camPos.x, dz = lp.z - camPos.z;
        if (dx * dx + dz * dz > 55.0f * 55.0f) continue;
        cand[c].l.pos = lp;
        cand[c].l.color = (Vector3){ 0.55f, 0.63f, 0.88f }; // cool moonlight-white
        cand[c].l.radius = 8.0f;
        cand[c].l.lampIndex = streetBase + i;
        cand[c].d2 = dx * dx + dz * dz;
        c++;
    }
    for (int i = 0; i < g->world.accentLampCount && c < 16; i++) {
        Vector3 lp = g->world.accentLamps[i];
        float dx = lp.x - camPos.x, dz = lp.z - camPos.z;
        if (dx * dx + dz * dz > 45.0f * 45.0f) continue;
        cand[c].l.pos = lp;
        cand[c].l.color = (i == 0) ? (Vector3){ 0.42f, 1.00f, 1.25f }  // cyan flood
                                    : (Vector3){ 0.72f, 0.58f, 1.20f }; // violet flood
        cand[c].l.radius = (i == 0) ? 9.5f : 9.0f;
        cand[c].l.lampIndex = accentBase + i;
        cand[c].d2 = dx * dx + dz * dz;
        c++;
    }

    // selection sort (tiny N), take nearest 8
    int n = c < 8 ? c : 8;
    for (int i = 0; i < n; i++) {
        int best = i;
        for (int j = i + 1; j < c; j++)
            if (cand[j].d2 < cand[best].d2) best = j;
        if (best != i) { PointLight tl = cand[i].l; float td = cand[i].d2;
                         cand[i].l = cand[best].l; cand[i].d2 = cand[best].d2;
                         cand[best].l = tl; cand[best].d2 = td; }
        out[i] = cand[i].l;
    }
    // tag each gathered light with its dynamic actor-atlas slot (0/1) if its
    // lamp is being re-rendered this frame, so the shader can shadow moving
    // bodies inside small light pools too
    for (int i = 0; i < n; i++)
        out[i].dynSlot = (out[i].lampIndex >= 0)
                       ? Shadow_LampDynSlot(&g->shadow, out[i].lampIndex) : -1;
    *count = n;
}

void Game_Draw(Game* g) {
    Weapon* w = &g->weapons[g->curWeapon];
    bool scoped = w->hasScope && g->adsBlend > 0.7f;

    BeginDrawing();
    // NOTE: do NOT draw any fullscreen 2D quad here before BeginMode3D.
    // A pre-3D fullscreen gradient was found (via spin+screenshot bisection)
    // to corrupt world rendering: dark ground, missing walls with floating
    // details, flicker/pop when the camera moves. Plain clear is safe.
    // Matches the sky dome horizon band so the void below the ground edge
    // never shows a color seam.
    ClearBackground((Color){ 195, 215, 232, 255 });

    // PASS 1: sun shadow map (casters only: no sky/grid/FX/decals/tracers).
    bool sh = g->shadow.available && g->shadow.enabled;
    Vector3 camFwd = Vector3Normalize(Vector3Subtract(g->camera.target, g->camera.position));
    if (sh) {
        Shadow_UpdateLight(&g->shadow, g->camera.position, camFwd);
        Shadow_BeginCasterPass(&g->shadow);
        // CLASSIC has hand-placed trees/props whose shadows are part of its
        // look. Random cities only need the collision boxes: their painted
        // pads, lane paint, grass and pebbles would just cost depth draws.
        if (g->world.mode == WORLD_CLASSIC)
            World_DrawLit(&g->world, g->gameTime, g->camera.position);
        else
            World_DrawCasters(&g->world);
        DrawBarrels(g->barrels, MAX_BARRELS, g->gameTime, false);
        Pickup_Draw(g->pickups, MAX_PICKUPS, g->camera.position);
        Enemy_DrawAll(g->enemies, MAX_ENEMIES, g->player.position,
                      g->camera.position, camFwd, g->gameTime, false);
        Player_DrawShadowBody(&g->player, g->yaw, g->pitch);
        Net_DrawBodies(&g->net, g->gameTime, false);
        Shadow_EndCasterPass(&g->shadow);

        // PASS 1b: dynamic actor atlas for the two nearest lamps (player +
        // enemies cast shadows inside small light pools, per frame)
        if (g->set.pointLights)
            Shadow_UpdateLampDyn(&g->shadow, g->camera.position, g->lampPos,
                                 g->lampCount, CastLampDynActors, g);
    }

    rlSetClipPlanes(0.08, 400.0);
    BeginMode3D(g->camera);
    World_DrawSky(g->gameTime, g->camera.position, g->world.limit);
    World_DrawGrid(&g->world);
    if (sh) {
        PointLight lights[8];
        int nl = 0;
        GatherLights(g, lights, &nl);
        g->shadow.fogEnabled = g->set.fog;
        Shadow_ApplyLit(&g->shadow, g->camera.position, lights, nl);
    }
    World_DrawLit(&g->world, g->gameTime, g->camera.position);
    DrawBarrels(g->barrels, MAX_BARRELS, g->gameTime, !sh);
    Pickup_Draw(g->pickups, MAX_PICKUPS, g->camera.position);
    Enemy_DrawAll(g->enemies, MAX_ENEMIES, g->player.position,
                  g->camera.position, camFwd, g->gameTime, !sh);
    Net_DrawBodies(&g->net, g->gameTime, true); // remote players (lit pass)
    if (sh) Shadow_Release(&g->shadow);
    FX_Draw(g->particles, MAX_PARTICLES, g->tracers, MAX_TRACERS, g->decals, MAX_DECALS);
    if (g->lastHitTimer > 0) {
        float t = g->lastHitTimer / 0.35f;
        float pulse = 0.05f + 0.05f * sinf(t * 40.0f);
        DrawSphereEx(g->lastHitPos, pulse + 0.02f, 8, 6, (Color){ 255, 80, 40, 255 });
    }
    EndMode3D();

    // match viewmodel FOV to main camera (fixes weapon size pop when ADS/scope)
    g->viewCam.fovy = g->camera.fovy;
    if (!scoped) {
        float spreadPx = 6.0f + (w->spreadHip * 900.0f) * (1.0f - g->adsBlend) + (g->moving ? 6.0f : 0.0f);
        UI_DrawCrosshair(spreadPx, g->adsActive, false);

        if (g->viewRTok) {
            float reloadT = 0;
            if (g->reloading) reloadT = ClampF(g->reloadPrevT, 0, 1);
            Vector3 voff = { g->sway.x + g->bob.x, g->sway.y + g->bob.y - g->landDip, g->recoilBack };
            // strafe lag: viewmodel rolls a few degrees opposite to the slide
            float lat = -g->player.velocity.x * cosf(g->yaw) + g->player.velocity.z * sinf(g->yaw);
            float moveRoll = ClampF(lat * 1.15f, -6.0f, 6.0f) * (1.0f - g->adsBlend);
            BeginTextureMode(g->viewRT);
            ClearBackground(BLANK);
            rlSetClipPlanes(0.05, 60.0);
            // first-person legs under the REAL world camera so they line up
            // with the ground and platforms (skipped while scoped)
            BeginMode3D(g->camera);
            Player_DrawLegs(&g->player, g->yaw, g->bobTime, g->moveSpeed,
                            g->player.grounded, g->adsBlend);
            EndMode3D();
            BeginMode3D(g->viewCam);
            // shotAge drives per-shot viewmodel animation (sniper bolt cycle)
            float shotAge = (g->fireCd > 0.0f) ? (w->fireRate - g->fireCd) : 999.0f;
            if (shotAge < 0.0f) shotAge = 999.0f;
            Weapon_DrawViewmodel(g->curWeapon, voff, g->muzzleTimer, reloadT,
                                 g->adsBlend, g->gameTime, g->sprintBlend, moveRoll, shotAge);
            EndMode3D();
            EndTextureMode();
            rlSetClipPlanes(0.08, 400.0);
            DrawTexturePro(g->viewRT.texture,
                (Rectangle){ 0, 0, (float)g->viewRT.texture.width, -(float)g->viewRT.texture.height },
                (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() },
                (Vector2){ 0, 0 }, 0, WHITE);
        }
    } else {
        Weapon_DrawScopeOverlay();
    }

    UI_DrawDamageVignette(g->damageFlash, g->player.health);
    // subtle vignette (post-3D overlay: cannot affect world rendering)
    {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangleGradientH(0, 0, 90, sh, (Color){ 0, 0, 0, 70 }, (Color){ 0, 0, 0, 0 });
    DrawRectangleGradientH(sw - 90, 0, 90, sh, (Color){ 0, 0, 0, 0 }, (Color){ 0, 0, 0, 70 });
    DrawRectangleGradientV(0, 0, sw, 70, (Color){ 0, 0, 0, 50 }, (Color){ 0, 0, 0, 0 });
    DrawRectangleGradientV(0, sh - 90, sw, 90, (Color){ 0, 0, 0, 0 }, (Color){ 0, 0, 0, 60 });
    }

    DamageFX_DrawIndicators(g->dmgInds, MAX_DAMAGE_INDICATORS, &g->player, g->yaw);
    DamageFX_DrawNumbers(g->dmgNums, MAX_DAMAGE_NUMBERS, g->camera);
    Weapon_DrawHitMarker(g->hitMarkerTimer, g->hitMarkerType);
    UI_DrawHUD(&g->player, w, g->weapons, g->curWeapon, g->reloading,
               g->reloading ? g->reloadPrevT : 0,
               g->kills, g->score, g->gameTime, g->adsBlend, g->net.active);
    UI_DrawNetHud(&g->net, g->camera, g->gameTime, g->kills);
    UI_DrawHeadshotBanner(g->headshotBanner, g->headshotStreak, g->headshots);

    if (g->paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 0, 0, 0, 120 });
        UI_DrawPauseMenu(&g->paused, &g->shouldQuit, g);
        if (!g->paused) SetMasterVolume(g->masterVol);
    }
    EndDrawing();
}

void Game_Unload(Game* g) {
    Net_Shutdown(&g->net);
    Shadow_Unload(&g->shadow);
    Audio_Unload(&g->audio);
    if (g->viewRTok) { UnloadRenderTexture(g->viewRT); g->viewRTok = false; }
}
