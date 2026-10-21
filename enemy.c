#include "enemy.h"
#include "damage_fx.h"
#include "world.h"
#include "player.h"
#include "rlgl.h"
#include <string.h>

static float SmoothStepHack(float t);

BoundingBox Enemy_BodyBox(const Enemy* e) {
    Vector3 h = { 0.35f, 0.9f, 0.35f };
    return (BoundingBox){ Vector3Subtract(e->position, h), Vector3Add(e->position, h) };
}

BoundingBox Enemy_HeadBox(const Enemy* e) {
    Vector3 c = { e->position.x, e->position.y + 0.72f, e->position.z };
    Vector3 h = { 0.19f, 0.18f, 0.18f };
    return (BoundingBox){ Vector3Subtract(c, h), Vector3Add(c, h) };
}

BoundingBox Enemy_LegBox(const Enemy* e) {
    float feetY = e->position.y - 0.9f;
    Vector3 c = { e->position.x, feetY + 0.35f, e->position.z };
    Vector3 h = { 0.30f, 0.35f, 0.30f };
    return (BoundingBox){ Vector3Subtract(c, h), Vector3Add(c, h) };
}

HitZone Enemy_ClassifyHit(const Enemy* e, Vector3 hitPos, bool headRayHit, float headDist, float bodyDist) {
    float feetY = e->position.y - 0.9f;
    float h = hitPos.y - feetY;
    // precise head ray wins
    if (headRayHit && headDist <= bodyDist + 0.08f) {
        if (e->hasHelmet) return HIT_HELMET;
        return HIT_HEAD;
    }
    if (h >= 1.42f) {
        if (e->hasHelmet) return HIT_HELMET;
        return HIT_HEAD;
    }
    if (h <= 0.62f) return HIT_LEG;
    return HIT_TORSO;
}

HitResult Enemy_ApplyDamage(Enemy* e, float dmg, HitZone zone) {
    HitResult r = { false, false, false };
    if (!e->alive) return r;
    if (zone == HIT_HELMET && e->hasHelmet) {
        // helmet absorbs the shot and pops off
        e->hasHelmet = false;
        r.helmetPopped = true;
        e->hurtTimer = 0.22f;
        e->flinchTimer = 0.18f;
        e->aimTimer = 0; // interrupt aim
        if (RandomFloat() < 0.4f) e->burstLeft = 0;
        // being shot at from an unknown direction: break sideways, alert
        if (e->dodgeTimer <= 0.0f) e->strafeDir *= -1.0f;
        e->dodgeTimer = fmaxf(e->dodgeTimer, 0.5f);
        e->alertTimer = fmaxf(e->alertTimer, 2.5f);
        return r;
    }
    e->health -= dmg;
    e->hurtTimer = 0.20f;
    e->flinchTimer = 0.16f;
    // flinch interrupts weak aim
    if (e->burstLeft == 0 && RandomFloat() < 0.55f) e->aimTimer *= 0.4f;
    // combat smarts: a hit means the shooter is close to our current facing;
    // dodge off the firing line and stay alert even without LOS
    if (e->dodgeTimer <= 0.0f) e->strafeDir *= -1.0f;
    e->dodgeTimer = fmaxf(e->dodgeTimer, 0.5f);
    e->alertTimer = fmaxf(e->alertTimer, 2.5f);
    if (e->health <= 0) {
        e->alive = false;
        e->deathTimer = 2.6f;
        e->respawnTimer = ENEMY_RESPAWN * RandRange(0.85f, 1.25f);
        e->burstLeft = 0;
        e->aimTimer = 0;
        e->windupTimer = 0;
        r.killed = true;
    } else {
        r.flinched = true;
    }
    return r;
}

void Enemy_InitAll(Enemy* enemies, int count) {
    Vector3 spawns[6] = {
        {  20.0f, 0.9f, -20.0f }, { -20.0f, 0.9f,  20.0f },
        {   0.0f, 0.9f, -35.0f }, {  30.0f, 0.9f,  10.0f },
        { -30.0f, 0.9f, -10.0f }, {   0.0f, 0.9f,  38.0f },
    };
    EnemyRole roles[6] = { ROLE_ASSAULT, ROLE_RUSHER, ROLE_MARKSMAN, ROLE_ASSAULT, ROLE_RUSHER, ROLE_MARKSMAN };
    int n = count < 6 ? count : 6;
    for (int i = 0; i < n; i++) {
        memset(&enemies[i], 0, sizeof(Enemy));
        enemies[i].position = spawns[i];
        enemies[i].spawnPoint = spawns[i];
        enemies[i].homePos = spawns[i];
        enemies[i].role = roles[i];
        enemies[i].elite = (i == 2 || i == 5);
        float hp = ENEMY_MAX_HEALTH;
        if (enemies[i].role == ROLE_RUSHER) hp *= 0.85f;
        if (enemies[i].role == ROLE_MARKSMAN) hp *= 0.9f;
        if (enemies[i].elite) hp *= 1.6f;
        if ((i % 2) == 0) hp += 10.0f;
        enemies[i].maxHealth = hp;
        enemies[i].health = hp;
        enemies[i].alive = true;
        enemies[i].hasHelmet = (i % 2 == 0) || enemies[i].elite;
        enemies[i].state = E_PATROL;
        enemies[i].strafeDir = (i & 1) ? 1.0f : -1.0f;
        enemies[i].facing = atan2f(-spawns[i].x, -spawns[i].z);
        float sm = 1.0f;
        if (enemies[i].role == ROLE_RUSHER) sm = 1.35f;
        else if (enemies[i].role == ROLE_MARKSMAN) sm = 0.82f;
        else sm = 0.95f + 0.1f * (float)(i % 3);
        enemies[i].speedMul = sm;
        enemies[i].dmgMul = enemies[i].elite ? 1.35f : (enemies[i].role == ROLE_MARKSMAN ? 1.2f : 1.0f);
        enemies[i].accMul = enemies[i].role == ROLE_MARKSMAN ? 1.3f : (enemies[i].role == ROLE_RUSHER ? 0.7f : 1.0f);
        if (enemies[i].elite) enemies[i].accMul *= 1.15f;
        enemies[i].lastPos = spawns[i];
        enemies[i].lastSeenPos = spawns[i];
        enemies[i].seed = i * 77;
    }
    for (int i = n; i < count; i++) {
        memset(&enemies[i], 0, sizeof(Enemy));
        enemies[i].alive = false;
        enemies[i].respawnTimer = 2.0f;
    }
}

void Enemy_SetActiveAll(Enemy* enemies, int count, bool active) {
    for (int i = 0; i < count; i++) {
        Enemy* e = &enemies[i];
        // drop any live bot / corpse and forget pending AI timers
        e->alive = false;
        e->health = 0;
        e->hurtTimer = 0;
        e->flinchTimer = 0;
        e->deathTimer = 0;
        e->muzzleFlash = 0;
        e->shootTimer = 0;
        e->aimTimer = 0;
        e->aimBlend = 0;
        e->windupTimer = 0;
        e->burstLeft = 0;
        e->burstTimer = 0;
        e->alertTimer = 0;
        e->visibleTimer = 0;
        e->dodgeTimer = 0;
        e->suppressTimer = 0;
        // re-enabling: respawn timers count down one after another so the
        // bots come back gradually instead of all popping in on one frame
        e->respawnTimer = active ? (0.6f + 0.55f * (float)i) : 0.0f;
    }
}

void Enemy_AlertAll(Enemy* enemies, int count, Vector3 gunPos, float radius) {
    for (int i = 0; i < count; i++) {
        if (!enemies[i].alive) continue;
        if (Vector3Distance(enemies[i].position, gunPos) < radius) {
            enemies[i].alertTimer = 3.5f;
            enemies[i].visibleTimer = fmaxf(enemies[i].visibleTimer, 0.4f);
            enemies[i].lastSeenPos = gunPos; // noise tells them where to look
            if (enemies[i].state == E_PATROL) enemies[i].state = E_CHASE;
        }
    }
}

static bool TryMove(Enemy* e, Vector3 delta, const Box* boxes, int boxCount) {
    Vector3 half = { 0.35f, 0.9f, 0.35f };
    Vector3 p = Vector3Add(e->position, delta);
    p.y = 0.9f;
    if (!World_MoveCollides(p, half, boxes, boxCount)) { e->position = p; return true; }
    Vector3 px = { e->position.x + delta.x, 0.9f, e->position.z };
    bool okx = !World_MoveCollides(px, half, boxes, boxCount);
    if (okx) e->position.x = px.x;
    Vector3 pz = { e->position.x, 0.9f, e->position.z + delta.z };
    bool okz = !World_MoveCollides(pz, half, boxes, boxCount);
    if (okz) e->position.z = pz.z;
    return okx || okz;
}

static float TurnToward(float cur, float target, float maxStep) {
    float d = target - cur;
    while (d > PI) d -= 2 * PI;
    while (d < -PI) d += 2 * PI;
    if (d > maxStep) d = maxStep;
    if (d < -maxStep) d = -maxStep;
    return cur + d;
}

static bool FindCover(Enemy* e, Vector3 playerEye, const Box* boxes, int boxCount, Vector3* out) {
    Vector3 best = { 0 };
    float bestScore = -1e9f;
    for (int k = 0; k < 10; k++) {
        float a = ((float)k / 10.0f) * 2 * PI + (float)e->seed * 0.13f;
        float r = 4.0f + (float)(k % 3) * 1.6f;
        Vector3 cand = { e->position.x + cosf(a) * r, 0.9f, e->position.z + sinf(a) * r };
        if (fabsf(cand.x) > MAP_LIMIT - 1 || fabsf(cand.z) > MAP_LIMIT - 1) continue;
        Vector3 half = { 0.35f, 0.9f, 0.35f };
        if (World_MoveCollides(cand, half, boxes, boxCount)) continue;
        Vector3 eye = { cand.x, cand.y + 0.55f, cand.z };
        bool blocked = !World_LOS(eye, playerEye, boxes, boxCount);
        float score = (blocked ? 100.0f : 0.0f) - Vector3Distance(cand, e->position) * 4.0f + RandomFloat() * 8.0f;
        if (score > bestScore) { bestScore = score; best = cand; }
    }
    if (bestScore > -1e8f) { *out = best; return bestScore > 50.0f; }
    return false;
}

void Enemy_UpdateAll(Enemy* enemies, int count, EnemyTarget* targets, int targetCount,
                     const Box* boxes, int boxCount, Audio* audio, float dt,
                     DamageIndicator* dmgInds, int maxDmgInds,
                     float* damageFlashOut, float gameTime) {
    if (targetCount < 1) return;
    float ramp = 1.0f + ClampF(gameTime / 60.0f * 0.08f, 0.0f, 0.6f);

    // count alive for pack tactics
    int aliveN = 0;
    for (int i = 0; i < count; i++) if (enemies[i].alive) aliveN++;

    for (int i = 0; i < count; i++) {
        Enemy* e = &enemies[i];
        if (e->hurtTimer > 0) e->hurtTimer -= dt;
        if (e->flinchTimer > 0) e->flinchTimer -= dt;
        if (e->muzzleFlash > 0) e->muzzleFlash -= dt;
        if (e->shootTimer > 0) e->shootTimer -= dt;
        if (e->meleeCd > 0) e->meleeCd -= dt;
        if (e->alertTimer > 0) e->alertTimer -= dt;
        if (e->burstTimer > 0) e->burstTimer -= dt;
        if (e->dodgeTimer > 0) e->dodgeTimer -= dt;
        if (e->suppressTimer > 0) e->suppressTimer -= dt;
        if (e->windupTimer > 0) e->windupTimer -= dt;
        // rifle raise/lower blend: 1 = shouldered while aiming or firing
        float aimTarget = (!e->alive) ? 0.0f
                        : ((e->aimTimer > 0.2f || e->burstLeft > 0) ? 1.0f : 0.0f);
        e->aimBlend += (aimTarget - e->aimBlend) * ClampF(dt * 7.0f, 0.0f, 1.0f);

        // pick this enemy's target: nearest living player, biased toward ones
        // it can actually see. The local player and each remote peer (via a
        // stub fed from its net state) compete here, per enemy, every frame.
        int tbest = 0;
        float tbestScore = 1e30f;
        {
            Vector3 ee = EnemyEyePos(e);
            float seer = (e->alertTimer > 0) ? 44.0f : 30.0f;
            if (e->role == ROLE_MARKSMAN) seer += 8.0f;
            for (int k = 0; k < targetCount; k++) {
                const EnemyTarget* t = &targets[k];
                if (t->player->health <= 0) continue;
                float d = Vector3DistanceSqr(e->position, t->player->position);
                float score = d;
                if (targetCount > 1 && (!e->alive || d > seer * seer ||
                    !World_LOS(ee, PlayerEyePos(t->player), boxes, boxCount)))
                    score += 400.0f; // unseen targets lose to visible ones
                if (score < tbestScore) { tbestScore = score; tbest = k; }
            }
        }
        const EnemyTarget* tgt = &targets[tbest];
        Player* player = tgt->player;          // this enemy's chosen opponent
        Vector3 playerEye = PlayerEyePos(player);

        if (!e->alive) {
            if (e->deathTimer > 0) e->deathTimer -= dt;
            else {
                e->respawnTimer -= dt;
                if (e->respawnTimer <= 0) {
                    Vector3 cand[6] = {
                        {  20, 0.9f, -20 }, { -20, 0.9f, 20 }, { 0, 0.9f, -35 },
                        {  30, 0.9f, 10 }, { -30, 0.9f, -10 }, { 0, 0.9f, 38 },
                    };
                    Vector3 best = cand[GetRandomValue(0, 5)];
                    float bd = -1;
                    for (int k = 0; k < 6; k++) {
                        float d = Vector3Distance(cand[k], player->position);
                        if (d > bd) { bd = d; best = cand[k]; }
                    }
                    if (bd < 12.0f) { e->respawnTimer = 1.0f; continue; }
                    e->alive = true;
                    e->maxHealth = ENEMY_MAX_HEALTH * ramp * (e->elite ? 1.6f : 1.0f);
                    e->health = e->maxHealth;
                    e->position = best;
                    e->homePos = best;
                    e->hasHelmet = (GetRandomValue(0, 1) == 0) || e->elite;
                    e->shootTimer = 0.6f;
                    e->aimTimer = 0;
                    e->aimBlend = 0;
                    e->visibleTimer = 0;
                    e->burstLeft = 0;
                    e->windupTimer = 0;
                    e->state = E_PATROL;
                    e->lastSeenPos = e->position;
                    e->suppressTimer = RandRange(0.5f, 1.5f);
                }
            }
            continue;
        }

        e->position.y = 0.9f;
        Vector3 toPlayer = { player->position.x - e->position.x, 0, player->position.z - e->position.z };
        float dist = Vector3Length(toPlayer);
        bool dead = (player->health <= 0);

        // vision cone: facing ±65°, wide if alerted/close
        float wantFace = (dist > 0.05f) ? atan2f(toPlayer.x, toPlayer.z) : e->facing;
        float cone = (e->alertTimer > 0 || dist < 7.0f) ? PI : 1.15f;
        float df = wantFace - e->facing;
        while (df > PI) df -= 2 * PI;
        while (df < -PI) df += 2 * PI;
        bool inCone = fabsf(df) < cone;
        float seeRange = (e->alertTimer > 0) ? 44.0f : 30.0f;
        if (e->role == ROLE_MARKSMAN) seeRange += 8.0f;
        bool los = !dead && dist < seeRange && World_LOS(EnemyEyePos(e), playerEye, boxes, boxCount);
        bool canSee = los && inCone;
        if (los && !inCone && dist < 6.0f) canSee = true; // peripheral at close range
        if (canSee) {
            e->alertTimer = 3.0f;
            e->lastSeenPos = player->position;
            e->visibleTimer += dt;
            // pack alert: shout to nearby allies (with the last known spot)
            for (int j = 0; j < count; j++) {
                if (j == i || !enemies[j].alive) continue;
                if (Vector3Distance(enemies[j].position, e->position) < 22.0f) {
                    enemies[j].alertTimer = fmaxf(enemies[j].alertTimer, 2.0f);
                    enemies[j].lastSeenPos = e->lastSeenPos;
                }
            }
        } else {
            e->visibleTimer = fmaxf(0, e->visibleTimer - dt * 2.0f);
        }

        bool reactive = e->visibleTimer > (e->role == ROLE_MARKSMAN ? 0.45f : 0.22f);
        bool inShootRange = !dead && reactive && dist <= ENEMY_SHOOT_RANGE && los;

        // separation
        Vector3 sep = { 0 };
        for (int j = 0; j < count; j++) {
            if (j == i || !enemies[j].alive) continue;
            Vector3 d = Vector3Subtract(e->position, enemies[j].position);
            d.y = 0;
            float l = Vector3Length(d);
            if (l > 0.01f && l < 2.4f) sep = Vector3Add(sep, Vector3Scale(Vector3Normalize(d), (2.4f - l) * 1.6f));
        }

        // dodge: sidestep burst when player recently fired (alert spike) or aimed at
        if (canSee && e->dodgeTimer <= 0 && e->state == E_STRAFE && RandomFloat() < dt * (e->role == ROLE_RUSHER ? 1.4f : 0.6f)) {
            e->strafeDir *= -1.0f;
            e->strafeTimer = RandRange(0.5f, 1.1f);
            e->dodgeTimer = RandRange(1.5f, 3.0f);
        }

        // --- MELEE with windup ---
        float meleeRange = (e->role == ROLE_RUSHER) ? ENEMY_MELEE_RANGE + 0.5f : ENEMY_MELEE_RANGE;
        float meleeCd = (e->role == ROLE_RUSHER) ? ENEMY_MELEE_CD * 0.65f : ENEMY_MELEE_CD;
        if (!dead && e->windupTimer <= 0 && dist < meleeRange + 0.9f && e->meleeCd <= 0) {
            e->windupTimer = 0.32f; // telegraph
            PlaySoundPitched(audio->shotAKR, 1.6f + RandomFloat() * 0.2f);
        }
        if (e->windupTimer > 0 && e->windupTimer - dt <= 0) {
            // strike moment
            e->meleeCd = meleeCd;
            e->muzzleFlash = 0.06f;
            if (!dead && dist < meleeRange + 0.7f) {
                float mdmg = ENEMY_MELEE_DMG * ramp * e->dmgMul * (e->role == ROLE_RUSHER ? 1.25f : 1.0f);
                if (Player_TakeDamage(player, mdmg)) {
                    if (tgt->isLocal) {
                        if (damageFlashOut) *damageFlashOut = 0.5f;
                        DamageFX_SpawnIndicator(dmgInds, maxDmgInds, e->position);
                        PlaySoundPitched(audio->playerHurt, 0.75f + RandomFloat() * 0.2f);
                    }
                    Vector3 away = (dist > 0.01f) ? Vector3Scale(toPlayer, -1.4f / dist) : (Vector3){ 0 };
                    player->position.x = ClampF(player->position.x + away.x * 0.18f, -MAP_LIMIT, MAP_LIMIT);
                    player->position.z = ClampF(player->position.z + away.z * 0.18f, -MAP_LIMIT, MAP_LIMIT);
                }
            }
        }

        // --- COVER / FLEE when weak ---
        bool weak = e->health < e->maxHealth * 0.32f;
        if (!dead && weak && los && e->role != ROLE_RUSHER) {
            if (e->state != E_COVER || !e->hasCoverPos) {
                if (FindCover(e, playerEye, boxes, boxCount, &e->coverPos)) {
                    e->hasCoverPos = true;
                    e->state = E_COVER;
                    e->aimTimer = 0;
                }
            }
        } else if (e->state == E_COVER && (!weak || !los)) {
            e->state = E_CHASE;
            e->hasCoverPos = false;
        }

        // --- MOVEMENT ---
        bool wantsMove = false;
        Vector3 wish = { 0 };
        float prefer = 11.0f;
        if (e->role == ROLE_MARKSMAN) prefer = 16.5f;
        else if (e->role == ROLE_RUSHER) prefer = 1.8f;
        else prefer = 10.5f + (float)(e->seed % 4);

        if (dead) {
            e->aimTimer = 0;
            Vector3 toHome = Vector3Subtract(e->homePos, e->position);
            toHome.y = 0;
            if (Vector3Length(toHome) > 2.0f) { wish = Vector3Normalize(toHome); wantsMove = true; }
            e->state = E_PATROL;
        } else if (e->state == E_COVER && e->hasCoverPos) {
            Vector3 d = Vector3Subtract(e->coverPos, e->position);
            d.y = 0;
            if (Vector3Length(d) > 0.8f) { wish = Vector3Normalize(d); wantsMove = true; }
            else {
                // crouch in cover: strafe minimally, peek occasionally
                e->strafeTimer -= dt;
                if (e->strafeTimer <= 0) { e->strafeTimer = RandRange(1.5f, 3.0f); }
            }
        } else if (!inShootRange || !los) {
            e->aimTimer = fmaxf(0, e->aimTimer - dt * 2.0f);
            e->state = E_CHASE;
            if (dist > 0.6f && (los || e->alertTimer > 0)) {
                wish = Vector3Normalize(toPlayer);
                // squad flanking: spread wider when several allies press at once
                float flank = ((i % 2) == 0 ? 0.40f : -0.40f) * (aliveN >= 3 ? 1.4f : 1.0f);
                Vector3 perp = { -wish.z, 0, wish.x };
                wish = Vector3Normalize(Vector3Add(wish, Vector3Scale(perp, flank)));
                wantsMove = true;
            } else if (e->alertTimer > 0) {
                // no LOS but alerted: sweep toward the last known position,
                // then search (orbit) the spot where the player was seen
                Vector3 d = Vector3Subtract(e->lastSeenPos, e->position);
                d.y = 0;
                float dd = Vector3Length(d);
                if (dd > 1.6f) {
                    wish = Vector3Normalize(d);
                    float flank = ((i % 2) == 0 ? 0.30f : -0.30f);
                    Vector3 perp = { -wish.z, 0, wish.x };
                    wish = Vector3Normalize(Vector3Add(wish, Vector3Scale(perp, flank)));
                    wantsMove = true;
                } else {
                    float t = gameTime * 0.55f + (float)e->seed;
                    Vector3 target = { e->lastSeenPos.x + sinf(t) * 4.5f, 0.9f,
                                       e->lastSeenPos.z + cosf(t * 1.3f) * 4.5f };
                    Vector3 d2 = Vector3Subtract(target, e->position);
                    d2.y = 0;
                    if (Vector3Length(d2) > 1.2f) { wish = Vector3Normalize(d2); wantsMove = true; }
                }
            } else {
                float t = gameTime * 0.3f + (float)e->seed;
                Vector3 target = { e->homePos.x + sinf(t) * 6.0f, 0.9f, e->homePos.z + cosf(t * 0.7f) * 6.0f };
                Vector3 d = Vector3Subtract(target, e->position);
                d.y = 0;
                if (Vector3Length(d) > 1.0f) { wish = Vector3Normalize(d); wantsMove = true; }
            }
        } else {
            e->state = E_STRAFE;
            e->strafeTimer -= dt;
            if (e->strafeTimer <= 0) {
                e->strafeTimer = RandRange(1.0f, 2.4f);
                // coordinate: avoid mirroring ally
                bool allySame = false;
                for (int j = 0; j < count; j++) {
                    if (j == i || !enemies[j].alive) continue;
                    if (enemies[j].state == E_STRAFE && enemies[j].strafeDir == e->strafeDir &&
                        Vector3Distance(enemies[j].position, e->position) < 14.0f) { allySame = true; break; }
                }
                if (allySame || RandomFloat() < 0.5f) e->strafeDir *= -1.0f;
            }
            Vector3 dir = (dist > 0.01f) ? Vector3Scale(toPlayer, 1.0f / dist) : (Vector3){ 0, 0, 1 };
            Vector3 perp = { -dir.z * e->strafeDir, 0, dir.x * e->strafeDir };
            float radial = 0;
            if (dist > prefer + 2.0f) radial = 0.9f;
            else if (dist < prefer - 1.5f) radial = -0.9f;
            wish = Vector3Add(Vector3Scale(perp, 1.0f), Vector3Scale(dir, radial));
            if (Vector3Length(wish) > 0.01f) wish = Vector3Normalize(wish);
            wantsMove = true;
        }

        // dodge impulse (AI v2): just got shot / strafing hot - break off the
        // firing line sideways for a moment (unless hunkered in cover)
        if (e->dodgeTimer > 0.22f && e->state != E_COVER && !dead) {
            Vector3 ddir = (dist > 0.01f) ? Vector3Scale(toPlayer, 1.0f / dist) : (Vector3){ 0, 0, 1 };
            Vector3 perp = { -ddir.z * e->strafeDir, 0, ddir.x * e->strafeDir };
            wish = Vector3Add(wish, Vector3Scale(perp, 1.15f));
            if (Vector3Length(wish) > 0.01f) wish = Vector3Normalize(wish);
            wantsMove = true;
        }

        if (wantsMove && e->windupTimer <= 0) {
            float spd = ENEMY_SPEED * e->speedMul * ramp;
            if (e->state == E_STRAFE) spd *= 0.8f;
            if (e->state == E_COVER) spd *= 1.05f;
            if (e->flinchTimer > 0) spd *= 0.45f;
            Vector3 delta = Vector3Scale(wish, spd * dt);
            delta = Vector3Add(delta, Vector3Scale(sep, dt * 2.2f));
            Vector3 before = e->position;
            TryMove(e, delta, boxes, boxCount);
            float moved = Vector3Distance(before, e->position);
            e->walkPhase += moved * 6.5f;
            if (moved < spd * dt * 0.22f) {
                e->stuckTimer += dt;
                if (e->stuckTimer > 0.65f) {
                    e->stuckTimer = 0;
                    e->strafeDir *= -1.0f;
                    Vector3 side = { -wish.z * e->strafeDir, 0, wish.x * e->strafeDir };
                    TryMove(e, Vector3Scale(side, spd * 0.6f), boxes, boxCount);
                }
            } else e->stuckTimer = 0;
            // face movement when not in combat, face player in combat
            float targetFace = (e->state == E_STRAFE || inShootRange) ? wantFace :
                               atan2f(wish.x, wish.z);
            e->facing = TurnToward(e->facing, targetFace, 6.0f * dt);
        } else {
            e->facing = TurnToward(e->facing, wantFace, 4.5f * dt);
        }

        // --- SUPPRESSION FIRE (AI v2) ---
        // Alerted, no clean shot: rake the last known position every few
        // seconds. It only hurts if the player is still near that spot with
        // real line of sight when the burst lands (punishes lazy re-peeks).
        if (!dead && !inShootRange && e->alertTimer > 0 && dist < 36.0f &&
            e->windupTimer <= 0 && e->suppressTimer <= 0) {
            e->suppressTimer = RandRange(2.2f, 3.8f) * (e->role == ROLE_RUSHER ? 0.75f : 1.0f);
            e->muzzleFlash = 0.06f;
            PlaySoundPitched(audio->shotAKR, 0.72f + RandomFloat() * 0.12f);
            bool stillThere = Vector3Distance(player->position, e->lastSeenPos) < 2.4f;
            bool realLos = !dead && World_LOS(EnemyEyePos(e), playerEye, boxes, boxCount);
            if (stillThere && realLos && dist < ENEMY_SHOOT_RANGE + 5.0f) {
                float dd = ENEMY_SHOOT_DMG * 0.6f * ramp * e->dmgMul;
                if (Player_TakeDamage(player, dd)) {
                    if (tgt->isLocal) {
                        if (damageFlashOut) *damageFlashOut = 0.4f;
                        DamageFX_SpawnIndicator(dmgInds, maxDmgInds, e->position);
                        PlaySoundPitched(audio->playerHurt, 0.8f + RandomFloat() * 0.15f);
                    }
                }
            }
        }

        // --- RANGED ATTACK (role bursts) ---
        float aimNeed = (e->role == ROLE_MARKSMAN) ? ENEMY_AIM_TIME + 0.25f : ENEMY_AIM_TIME;
        if (e->elite) aimNeed *= 0.8f;
        if (inShootRange && e->windupTimer <= 0) {
            if (e->burstLeft > 0) {
                if (e->burstTimer <= 0) {
                    e->burstTimer = (e->role == ROLE_MARKSMAN) ? 0.22f : 0.13f;
                    e->burstLeft--;
                    e->muzzleFlash = 0.06f;
                    float acc = 1.0f - (dist / ENEMY_SHOOT_RANGE) * 0.85f;
                    acc *= 0.8f * e->accMul;
                    if (aliveN >= 3) acc *= 1.08f; // pack focus
                    // AI v2 tracking: lead the player's lateral motion - fast
                    // strafing is hard to hit, standing still is punished, and
                    // the longer they have eyes on you the more dialed-in they
                    // become (visibleTimer bonus).
                    Vector3 pv = player->velocity;
                    pv.y = 0;
                    float pSpeed = Vector3Length(pv);
                    float latSpeed = 0.0f;
                    if (dist > 0.01f) {
                        Vector3 dirP = { toPlayer.x / dist, 0, toPlayer.z / dist };
                        Vector3 latV = Vector3Subtract(pv, Vector3Scale(dirP, Vector3DotProduct(pv, dirP)));
                        latSpeed = Vector3Length(latV);
                    }
                    acc *= 1.0f - ClampF(latSpeed / 16.0f, 0.0f, 0.30f);
                    acc *= 1.0f + ClampF(e->visibleTimer * 0.04f, 0.0f, 0.18f);
                    if (pSpeed < 0.5f) acc *= 1.15f; // camping is punished
                    if (tgt->moving) acc *= 0.85f;
                    if (tgt->airborne) acc *= 0.62f;
                    if (Vector3Length(sep) > 0.5f) acc *= 0.85f;
                    if (e->flinchTimer > 0) acc *= 0.6f;
                    acc = ClampF(acc, 0.05f, 0.92f);
                    if (RandomFloat() < acc) {
                        float dd = ENEMY_SHOOT_DMG * ramp * e->dmgMul;
                        if (Player_TakeDamage(player, dd)) {
                            if (tgt->isLocal) {
                                if (damageFlashOut) *damageFlashOut = 0.35f;
                                DamageFX_SpawnIndicator(dmgInds, maxDmgInds, e->position);
                                PlaySoundPitched(audio->playerHurt, 0.85f + RandomFloat() * 0.15f);
                            }
                        }
                    }
                    PlaySoundPitched(audio->shotAKR, 0.7f + RandomFloat() * 0.1f);
                }
            } else if (e->aimTimer < aimNeed) {
                e->aimTimer += dt;
            } else if (e->shootTimer <= 0) {
                if (e->role == ROLE_MARKSMAN) e->burstLeft = 1 + GetRandomValue(0, 1);
                else if (e->role == ROLE_RUSHER) e->burstLeft = 3 + GetRandomValue(0, 3);
                else e->burstLeft = 2 + GetRandomValue(0, 2);
                e->burstTimer = 0;
                e->shootTimer = ENEMY_SHOOT_CD * RandRange(0.9f, 1.3f) / ramp;
                if (e->role == ROLE_RUSHER) e->shootTimer *= 0.8f;
                e->aimTimer = 0.15f;
            }
        } else {
            if (e->burstLeft == 0) e->aimTimer = fmaxf(0, e->aimTimer - dt * 3.0f);
        }
    }
}

void Enemy_DrawAll(const Enemy* enemies, int count, Vector3 playerPos, float time, bool fakeBlobs) {
    (void)time;
    for (int i = 0; i < count; i++) {
        const Enemy* e = &enemies[i];
        // corpse
        if (!e->alive) {
            if (e->deathTimer <= 0) continue;
            float k = 1.0f - e->deathTimer / 2.6f; // 0 fresh -> 1 gone
            float fall = SmoothStepHack(ClampF(k * 2.2f, 0, 1));
            rlPushMatrix();
            rlTranslatef(e->position.x, 0.25f, e->position.z);
            rlRotatef(e->facing * RAD2DEG, 0, 1, 0);
            rlRotatef(90.0f * fall, 1, 0, 0);
            // Opaque corpse (transparent fade corrupts depth and makes it vanish early)
            DrawCubeV((Vector3){ 0, 0.2f, 0 }, (Vector3){ 0.54f, 0.66f, 0.34f }, (Color){ 120, 60, 55, 255 });
            DrawCubeV((Vector3){ 0, 0.72f, 0 }, (Vector3){ 0.30f, 0.30f, 0.28f }, (Color){ 150, 120, 100, 255 });
            if (e->hasHelmet) DrawSphereEx((Vector3){ 0, 0.95f, 0 }, 0.16f, 8, 6, (Color){ 60, 62, 66, 255 });
            rlPopMatrix();
            // blood pool above roads, no depth write (flush batch first!)
            rlDrawRenderBatchActive();
            rlDisableDepthMask();
            DrawCylinder((Vector3){ e->position.x, 0.16f, e->position.z }, 0.7f * (0.5f + k * 0.5f), 0.7f * (0.5f + k * 0.5f), 0.02f, 12,
                         (Color){ 140, 10, 10, (unsigned char)(160 * (1 - k * 0.5f)) });
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            continue;
        }
        bool hurt = e->hurtTimer > 0.0f;
        bool aiming = e->aimTimer > 0.3f || e->burstLeft > 0;
        bool winding = e->windupTimer > 0;
        Color skinCol  = hurt ? WHITE : (Color){ 214, 168, 128, 255 };
        Color bodyCol  = hurt ? WHITE : e->elite ? (Color){ 90, 30, 32, 255 } :
                         (e->role == ROLE_RUSHER ? (Color){ 130, 75, 40, 255 } :
                         (e->role == ROLE_MARKSMAN ? (Color){ 45, 70, 60, 255 } : (i & 1) ? (Color){ 120, 60, 55, 255 } : (Color){ 52, 82, 138, 255 }));
        Color pantCol  = hurt ? WHITE : (Color){ 38, 42, 52, 255 };
        Color darkCol  = hurt ? WHITE : (Color){ 18, 20, 26, 255 };
        Color gunCol   = hurt ? WHITE : (Color){ 34, 36, 42, 255 };
        Color gunSteel = hurt ? WHITE : (Color){ 90, 95, 105, 255 };

        float angle = e->facing * RAD2DEG;
        float flinchLean = (e->flinchTimer > 0) ? (e->flinchTimer / 0.16f) * -0.12f : 0;
        float windLean = winding ? 0.18f : 0;
        float crouch = (e->state == E_COVER) ? 0.18f
                     : ((e->role == ROLE_MARKSMAN && aiming) ? 0.10f : 0);
        // strafe roll: lean the body into sidestep movement (deg, about local z)
        float rollLean = (e->state == E_STRAFE) ? e->strafeDir * 6.0f : 0.0f;

        // Soft blob shadow: test on, write off (never erases ground/props
        // behind it). Only used when the real shadow map is unavailable.
        if (fakeBlobs) {
            rlDrawRenderBatchActive();
            rlDisableDepthMask();
            DrawCylinder((Vector3){ e->position.x, 0.14f, e->position.z }, 0.55f, 0.55f, 0.02f, 12, (Color){ 0, 0, 0, 70 });
            DrawCylinder((Vector3){ e->position.x, 0.15f, e->position.z }, 0.75f, 0.75f, 0.012f, 12, (Color){ 0, 0, 0, 30 });
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
        }

        rlPushMatrix();
        rlTranslatef(e->position.x, e->position.y - crouch, e->position.z);
        rlRotatef(angle, 0, 1, 0);
        rlRotatef(rollLean, 0, 0, 1);
        rlTranslatef(0, 0, flinchLean + windLean * 0.3f);

        float legSwing = sinf(e->walkPhase) * 0.17f;
        if (winding) legSwing *= 0.3f;
        float bob = sinf(e->walkPhase * 2.0f) * 0.02f;

        // legs
        DrawCubeV((Vector3){ -0.14f, -0.55f, legSwing }, (Vector3){ 0.20f, 0.58f, 0.22f }, pantCol);
        DrawCubeV((Vector3){  0.14f, -0.55f, -legSwing }, (Vector3){ 0.20f, 0.58f, 0.22f }, pantCol);
        DrawCubeV((Vector3){ -0.14f, -0.87f, 0.04f + legSwing }, (Vector3){ 0.22f, 0.10f, 0.32f }, darkCol);
        DrawCubeV((Vector3){  0.14f, -0.87f, 0.04f - legSwing }, (Vector3){ 0.22f, 0.10f, 0.32f }, darkCol);

        // upper body (bobs with the gait, leans forward while aiming)
        rlPushMatrix();
        rlTranslatef(0, bob, 0);
        rlRotatef(3.0f * e->aimBlend, 1, 0, 0);
        // torso (vest for elite)
        DrawCubeV((Vector3){ 0, 0.20f, 0 }, (Vector3){ 0.54f, 0.66f, 0.34f }, bodyCol);
        DrawCubeWiresV((Vector3){ 0, 0.20f, 0 }, (Vector3){ 0.54f, 0.66f, 0.34f }, darkCol);
        if (e->elite) {
            DrawCubeV((Vector3){ 0, 0.22f, 0.18f }, (Vector3){ 0.40f, 0.45f, 0.03f }, (Color){ 30, 30, 34, 255 });
            DrawCubeV((Vector3){ -0.30f, 0.42f, 0 }, (Vector3){ 0.10f, 0.10f, 0.30f }, (Color){ 150, 30, 30, 255 });
            DrawCubeV((Vector3){ 0.30f, 0.42f, 0 }, (Vector3){ 0.10f, 0.10f, 0.30f }, (Color){ 150, 30, 30, 255 });
        }
        DrawCubeV((Vector3){ 0, -0.10f, 0 }, (Vector3){ 0.55f, 0.06f, 0.35f }, darkCol);
        // neck + head + helmet
        DrawCubeV((Vector3){ 0, 0.54f, 0 }, (Vector3){ 0.12f, 0.08f, 0.12f }, skinCol);
        DrawCubeV((Vector3){ 0, 0.72f, 0 }, (Vector3){ 0.30f, 0.30f, 0.28f }, skinCol);
        if (e->hasHelmet) {
            DrawCubeV((Vector3){ 0, 0.90f, 0 }, (Vector3){ 0.36f, 0.12f, 0.34f }, e->elite ? (Color){ 90, 20, 20, 255 } : (Color){ 58, 60, 58, 255 });
            DrawCubeV((Vector3){ 0, 0.82f, -0.02f }, (Vector3){ 0.38f, 0.04f, 0.36f }, (Color){ 30, 30, 30, 255 });
        } else {
            DrawCubeV((Vector3){ 0, 0.90f, 0 }, (Vector3){ 0.32f, 0.06f, 0.30f }, (Color){ 45, 32, 24, 255 });
        }
        Color eyeC = (aiming || winding) ? (Color){ 255, 60, 40, 255 } : darkCol;
        if (e->elite) eyeC = (Color){ 255, 40, 40, 255 };
        DrawCubeV((Vector3){ -0.07f, 0.74f, 0.145f }, (Vector3){ 0.05f, 0.06f, 0.012f }, eyeC);
        DrawCubeV((Vector3){  0.07f, 0.74f, 0.145f }, (Vector3){ 0.05f, 0.06f, 0.012f }, eyeC);
        if (e->elite) {
            DrawCubeV((Vector3){ 0, 0.74f, 0.15f }, (Vector3){ 0.22f, 0.03f, 0.01f }, (Color){ 255, 40, 40, 200 });
        }

        // ---- rifle held two-handed, pitching up/down to track the player ----
        const float pvY = 0.30f, pvZ = 0.18f;
        float aimPitch = 0.0f;
        if (!winding && e->aimBlend > 0.001f) {
            Vector3 gp = { e->position.x, e->position.y - crouch + pvY + bob, e->position.z + pvZ };
            float dy = (playerPos.y + 1.1f) - gp.y;
            float dx = playerPos.x - gp.x, dz = playerPos.z - gp.z;
            float horiz = sqrtf(dx * dx + dz * dz);
            aimPitch = ClampF(-atan2f(dy, fmaxf(horiz, 0.01f)) * RAD2DEG, -40.0f, 40.0f) * e->aimBlend;
        }
        float lift = 0.10f * e->aimBlend;

        rlPushMatrix();
        rlTranslatef(0, lift, 0);
        rlTranslatef(0, pvY, pvZ);
        rlRotatef(aimPitch, 1, 0, 0);
        rlTranslatef(0, -pvY, -pvZ);
        // stock / receiver / rail / sights
        DrawCubeV((Vector3){ 0.02f, -0.03f, 0.16f }, (Vector3){ 0.09f, 0.13f, 0.24f }, gunCol);
        DrawCubeV((Vector3){ 0, 0.02f, 0.52f }, (Vector3){ 0.11f, 0.13f, 0.55f }, gunCol);
        DrawCubeWiresV((Vector3){ 0, 0.02f, 0.52f }, (Vector3){ 0.11f, 0.13f, 0.55f }, darkCol);
        DrawCubeV((Vector3){ 0, 0.095f, 0.52f }, (Vector3){ 0.05f, 0.015f, 0.50f }, gunSteel);
        DrawCubeV((Vector3){ 0, 0.135f, 0.60f }, (Vector3){ 0.05f, 0.05f, 0.03f }, gunSteel);
        DrawCubeV((Vector3){ 0, 0.135f, 1.04f }, (Vector3){ 0.02f, 0.06f, 0.03f }, gunSteel);
        if (e->elite) DrawCubeV((Vector3){ 0, 0.02f, 0.52f }, (Vector3){ 0.115f, 0.03f, 0.34f }, (Color){ 150, 30, 30, 255 });
        // barrel / muzzle
        DrawCylinderEx((Vector3){ 0, 0.035f, 0.80f }, (Vector3){ 0, 0.035f, 1.18f }, 0.021f, 0.021f, 8, gunSteel);
        DrawCubeV((Vector3){ 0, 0.035f, 1.20f }, (Vector3){ 0.05f, 0.05f, 0.05f }, darkCol);
        // handguard / mag / grip
        DrawCubeV((Vector3){ 0, -0.04f, 0.80f }, (Vector3){ 0.09f, 0.08f, 0.34f }, gunCol);
        DrawCubeV((Vector3){ 0, -0.16f, 0.46f }, (Vector3){ 0.08f, 0.18f, 0.12f }, darkCol);
        DrawCubeV((Vector3){ 0, -0.27f, 0.49f }, (Vector3){ 0.08f, 0.10f, 0.12f }, darkCol);
        DrawCubeV((Vector3){ 0, -0.11f, 0.34f }, (Vector3){ 0.07f, 0.16f, 0.09f }, gunCol);
        // hands gripping the rifle (follow the pitch)
        if (!winding) DrawCubeV((Vector3){ 0.09f, -0.08f, 0.40f }, (Vector3){ 0.13f, 0.13f, 0.13f }, skinCol);
        DrawCubeV((Vector3){ -0.09f, -0.05f, 0.80f }, (Vector3){ 0.13f, 0.13f, 0.13f }, skinCol);
        if (aiming && e->burstLeft == 0 && !winding) {
            DrawCubeV((Vector3){ 0, 0.035f, 0.95f }, (Vector3){ 0.008f, 0.008f, 0.5f }, (Color){ 255, 60, 40, 120 });
        }
        if (e->muzzleFlash > 0.0f) {
            float t = e->muzzleFlash / 0.06f;
            float s = 0.06f * (0.5f + 0.5f * t) * (0.7f + RandomFloat() * 0.6f);
            Vector3 fp = { 0, 0.035f, 1.26f };
            DrawSphereEx(fp, s * 1.4f, 8, 6, (Color){ 255, 210, 90, 255 });
            DrawSphereEx(fp, s * 0.7f, 8, 6, (Color){ 255, 255, 240, 255 });
        }
        rlPopMatrix(); // rifle pitch

        // arms: shoulders -> elbows -> hands (hands track the rifle pitch)
        {
            float th = aimPitch * DEG2RAD;
            float cy = cosf(th), sy = sinf(th);
            Vector3 hr = { 0.09f, -0.08f - pvY, 0.40f - pvZ };
            Vector3 hl = { -0.09f, -0.05f - pvY, 0.80f - pvZ };
            Vector3 hrw = { hr.x, pvY + hr.y * cy - hr.z * sy + lift, pvZ + hr.y * sy + hr.z * cy };
            Vector3 hlw = { hl.x, pvY + hl.y * cy - hl.z * sy + lift, pvZ + hl.y * sy + hl.z * cy };
            if (winding) {
                // melee: right arm cocks back high for a haymaker
                Vector3 fist = { 0.16f, 0.56f, 0.40f };
                DrawLimbBlocky((Vector3){ 0.30f, 0.42f, 0.04f }, (Vector3){ 0.38f, 0.16f, 0.18f }, 0.15f, bodyCol);
                DrawLimbBlocky((Vector3){ 0.38f, 0.16f, 0.18f }, fist, 0.135f, bodyCol);
                DrawCubeV(fist, (Vector3){ 0.14f, 0.14f, 0.14f }, skinCol);
            } else {
                DrawLimbBlocky((Vector3){ 0.30f, 0.42f, 0.04f }, (Vector3){ 0.35f, 0.14f, 0.26f }, 0.15f, bodyCol);
                DrawLimbBlocky((Vector3){ 0.35f, 0.14f, 0.26f }, hrw, 0.135f, bodyCol);
            }
            DrawLimbBlocky((Vector3){ -0.30f, 0.42f, 0.04f }, (Vector3){ -0.35f, 0.14f, 0.26f }, 0.15f, bodyCol);
            DrawLimbBlocky((Vector3){ -0.35f, 0.14f, 0.26f }, hlw, 0.135f, bodyCol);
        }
        rlPopMatrix(); // upper-body bob
        rlPopMatrix(); // enemy body

        float d = Vector3Distance(e->position, playerPos);
        if (d < 42.0f) {
            Vector3 bar = { e->position.x, e->position.y + 1.12f, e->position.z };
            DrawCube(bar, 0.9f, 0.08f, 0.08f, BLACK);
            float f = ClampF(e->health / e->maxHealth, 0, 1);
            Color hc = e->elite ? GOLD : (f > 0.5f ? GREEN : (f > 0.25f ? ORANGE : RED));
            if (f > 0)
                DrawCube((Vector3){ bar.x - 0.45f + 0.45f * f, bar.y, bar.z }, 0.9f * f, 0.08f, 0.08f, hc);
        }
    }
}

// smoothstep helper (local, avoids extra header dep)
static float SmoothStepHack(float t) {
    t = ClampF(t, 0, 1);
    return t * t * (3 - 2 * t);
}
