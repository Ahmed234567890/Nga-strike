#include "player.h"
#include "audio.h"

void Player_Init(Player* p) {
    p->position = (Vector3){ 0, 0, 0 };
    p->velocity = (Vector3){ 0, 0, 0 };
    p->grounded = true;
    p->health = PLAYER_MAX_HEALTH;
    p->spawnProtect = 0.0f;
}

void Player_Respawn(Player* p) {
    p->position = (Vector3){ 0, 0, 0 };
    p->velocity = (Vector3){ 0, 0, 0 };
    p->health = PLAYER_MAX_HEALTH;
    p->spawnProtect = 2.0f;
}

bool Player_TakeDamage(Player* p, float dmg) {
    if (p->spawnProtect > 0.0f) return false;
    if (p->health <= 0.0f) return false;
    p->health -= dmg;
    return true;
}

void Player_DrawShadowBody(const Player* p, float yaw, float pitch) {
    rlPushMatrix();
    rlTranslatef(p->position.x, p->position.y, p->position.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    // slight torso lean matching view pitch (clamped, keeps the silhouette
    // plausible without throwing the shadow off the feet)
    float lean = ClampF(-pitch * RAD2DEG * 0.22f, -14.0f, 14.0f);
    rlRotatef(lean, 1, 0, 0);
    // legs
    DrawCubeV((Vector3){ -0.15f, 0.42f, 0 }, (Vector3){ 0.21f, 0.84f, 0.23f }, WHITE);
    DrawCubeV((Vector3){  0.15f, 0.42f, 0 }, (Vector3){ 0.21f, 0.84f, 0.23f }, WHITE);
    // torso
    DrawCubeV((Vector3){ 0, 1.24f, 0 }, (Vector3){ 0.55f, 0.74f, 0.34f }, WHITE);
    // head
    DrawCubeV((Vector3){ 0, 1.78f, 0 }, (Vector3){ 0.30f, 0.30f, 0.28f }, WHITE);
    // arms reaching forward onto the rifle
    DrawCubeV((Vector3){ -0.29f, 1.30f, 0.12f }, (Vector3){ 0.16f, 0.16f, 0.40f }, WHITE);
    DrawCubeV((Vector3){  0.29f, 1.30f, 0.12f }, (Vector3){ 0.16f, 0.16f, 0.40f }, WHITE);
    // rifle held high across the silhouette
    DrawCubeV((Vector3){ -0.02f, 1.36f, 0.62f }, (Vector3){ 0.13f, 0.13f, 1.05f }, WHITE);
    rlPopMatrix();
}

void Player_Update(Player* p, const Box* boxes, int boxCount, float mapLimit, float yaw,
                   float dt, bool ads, bool sprintWant, bool inputEnabled,
                   Vector2* bobOut, bool* movingOut, float* bobTime, float* speedOut) {
    if (p->spawnProtect > 0.0f) p->spawnProtect -= dt;

    Vector3 flatForward = { sinf(yaw), 0.0f, cosf(yaw) };
    Vector3 right       = { -cosf(yaw), 0.0f, sinf(yaw) };

    Vector3 wish = { 0 };
    if (inputEnabled) {
        if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, flatForward);
        if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, flatForward);
        if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
        if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    }
    bool hasInput = Vector3Length(wish) > 0.001f;
    if (hasInput) wish = Vector3Normalize(wish);

    // sprint only pushing forward, grounded, not aiming
    bool sprinting = sprintWant && hasInput && !ads && p->grounded &&
                     Vector3DotProduct(wish, flatForward) > 0.35f;
    float maxSpeed = ads ? PLAYER_ADS_SPEED
                         : (sprinting ? PLAYER_SPRINT_SPEED : PLAYER_WALK_SPEED);

    // acceleration model: exponential approach to target velocity. Grounded
    // input reaches ~90% speed in ~0.3s; releasing coasts to a stop; in the
    // air velocity is preserved (momentum) with only gentle steering.
    Vector2 vel = { p->velocity.x, p->velocity.z };
    if (p->grounded) {
        float rate = hasInput ? 7.5f : 6.0f;
        float f = 1.0f - expf(-rate * dt);
        vel.x += (wish.x * maxSpeed - vel.x) * f;
        vel.y += (wish.z * maxSpeed - vel.y) * f;
    } else if (hasInput) {
        float cur = sqrtf(vel.x*vel.x + vel.y*vel.y);
        if (cur < PLAYER_WALK_SPEED * 0.9f) cur = PLAYER_WALK_SPEED * 0.9f;
        float f = 1.0f - expf(-2.2f * dt);
        vel.x += (wish.x * cur - vel.x) * f;
        vel.y += (wish.z * cur - vel.y) * f;
    }
    p->velocity.x = vel.x;
    p->velocity.z = vel.y;

    Vector3 move = { p->velocity.x * dt, 0.0f, p->velocity.z * dt };

    p->velocity.y += GRAVITY * dt;

    Vector3 half = { PLAYER_RADIUS, PLAYER_EYE_HEIGHT * 0.5f, PLAYER_RADIUS };
    Vector3 center = { p->position.x, p->position.y + half.y, p->position.z };

    // X (with auto step-up on curbs/stairs)
    float nx = center.x + move.x;
    if (!World_MoveCollides((Vector3){ nx, center.y, center.z }, half, boxes, boxCount)) {
        center.x = nx;
    } else if (p->grounded && fabsf(move.x) > 1e-5f &&
               !World_MoveCollides((Vector3){ nx, center.y + PLAYER_STEP_UP, center.z }, half, boxes, boxCount)) {
        center.x = nx;
        p->position.y += PLAYER_STEP_UP; // ground resolve snaps back down
    } else {
        p->velocity.x = 0;
    }
    // Z (with auto step-up)
    float nz = center.z + move.z;
    if (!World_MoveCollides((Vector3){ center.x, center.y, nz }, half, boxes, boxCount)) {
        center.z = nz;
    } else if (p->grounded && fabsf(move.z) > 1e-5f &&
               !World_MoveCollides((Vector3){ center.x, center.y + PLAYER_STEP_UP, nz }, half, boxes, boxCount)) {
        center.z = nz;
        p->position.y += PLAYER_STEP_UP;
    } else {
        p->velocity.z = 0;
    }
    p->position.x = center.x;
    p->position.z = center.z;

    p->position.x = ClampF(p->position.x, -mapLimit, mapLimit);
    p->position.z = ClampF(p->position.z, -mapLimit, mapLimit);

    // Y (resolve highest ground, ceilings)
    p->position.y += p->velocity.y * dt;
    center.y = p->position.y + half.y;
    center.x = p->position.x;
    center.z = p->position.z;

    p->grounded = false;
    float bestTop = -1e9f;
    float bestBottom = 1e9f;
    bool hit = false;
    for (int i = 0; i < boxCount; i++) {
        if (AABBvsAABB(center, half, boxes[i].center, boxes[i].halfExtents)) {
            hit = true;
            float top = boxes[i].center.y + boxes[i].halfExtents.y;
            float bot = boxes[i].center.y - boxes[i].halfExtents.y;
            if (p->velocity.y <= 0.05f && top > bestTop) bestTop = top;
            if (p->velocity.y > 0.05f && bot < bestBottom) bestBottom = bot;
        }
    }
    if (hit) {
        if (p->velocity.y <= 0.05f && bestTop > -1e8f) {
            p->position.y = bestTop;
            p->velocity.y = 0;
            p->grounded = true;
        } else if (p->velocity.y > 0.05f && bestBottom < 1e8f) {
            p->position.y = bestBottom - PLAYER_EYE_HEIGHT - 0.001f;
            if (p->position.y < 0) p->position.y = 0;
            p->velocity.y = 0;
        }
    }
    if (p->position.y <= 0.0f) {
        p->position.y = 0;
        p->velocity.y = 0;
        p->grounded = true;
    }
    // keep the eye out of ceilings after a step-up nudge
    if (p->grounded && bestTop > -1e8f && p->position.y > bestTop + 0.01f) {
        p->position.y = bestTop;
    }

    float hSpeed = sqrtf(p->velocity.x * p->velocity.x + p->velocity.z * p->velocity.z);
    bool moving = hSpeed > 0.6f;

    // head-bob values for viewmodel (phase scales with real speed)
    Vector2 bob = { 0, 0 };
    if (moving && p->grounded) {
        *bobTime += dt * hSpeed;
        float amp = 0.0055f + 0.0035f * ClampF((hSpeed - PLAYER_WALK_SPEED) / 4.4f, 0.0f, 1.0f);
        bob.x = sinf(*bobTime * 1.0f) * amp;
        bob.y = fabsf(cosf(*bobTime * 1.0f)) * amp * 1.1f;
    } else {
        *bobTime = 0;
    }
    if (bobOut) *bobOut = bob;
    if (movingOut) *movingOut = moving;
    if (speedOut) *speedOut = hSpeed;
}

// First-person legs. Drawn with the world camera inside the viewmodel
// render target, so they plant on the real ground. Alpha fades when aiming.
void Player_DrawLegs(const Player* p, float yaw, float bobTime, float speed,
                     bool grounded, float adsBlend) {
    if (adsBlend > 0.55f) return;
    unsigned char a = 255;
    if (adsBlend > 0.10f)
        a = (unsigned char)(255.0f - ClampF((adsBlend - 0.10f) / 0.45f, 0.0f, 1.0f) * 235.0f);
    if (a < 24) return;

    Color pants    = (Color){ 58, 62, 74, a };
    Color pantsDk  = (Color){ 41, 44, 53, a };
    Color boot     = (Color){ 36, 31, 27, a };
    Color sole     = (Color){ 20, 19, 18, a };
    Color knee     = (Color){ 31, 33, 40, a };
    Color strap    = (Color){ 28, 30, 36, a };

    float hs = speed;
    float amp = grounded ? ClampF(0.14f + hs * 0.048f, 0.10f, 0.78f) : 0.0f;
    float swingScale = grounded ? ClampF(hs / PLAYER_WALK_SPEED, 0.0f, 1.55f) : 0.0f;

    rlPushMatrix();
    rlTranslatef(p->position.x, p->position.y, p->position.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    for (int leg = 0; leg < 2; leg++) {
        float sideX = (leg == 0) ? -0.16f : 0.16f;
        float swing;
        if (!grounded) {
            swing = (leg == 0) ? -0.42f : 0.30f; // tucked in the air
        } else {
            swing = sinf(bobTime + (leg == 0 ? PI : 0.0f)) * amp * swingScale;
        }
        rlPushMatrix();
        rlTranslatef(sideX, 0.94f, 0.0f);            // hip joint
        rlRotatef(swing * RAD2DEG, 1, 0, 0);
        // hip/pelvis mass + thigh
        DrawCubeV((Vector3){ 0, -0.02f, 0 }, (Vector3){ 0.225f, 0.14f, 0.24f }, strap);
        DrawCubeV((Vector3){ 0, -0.24f, 0.015f }, (Vector3){ 0.195f, 0.44f, 0.215f }, pants);
        rlTranslatef(0, -0.48f, 0);
        rlRotatef(-swing * 1.30f * RAD2DEG, 1, 0, 0); // shin stays near vertical
        // knee pad + shin
        DrawCubeV((Vector3){ 0, 0.015f, 0.075f }, (Vector3){ 0.175f, 0.165f, 0.15f }, knee);
        DrawCubeV((Vector3){ 0, -0.19f, -0.015f }, (Vector3){ 0.175f, 0.38f, 0.195f }, pantsDk);
        // boot + sole
        DrawCubeV((Vector3){ 0, -0.385f, 0.055f }, (Vector3){ 0.19f, 0.15f, 0.30f }, boot);
        DrawCubeV((Vector3){ 0, -0.462f, 0.06f }, (Vector3){ 0.205f, 0.05f, 0.325f }, sole);
        rlPopMatrix();
    }
    rlPopMatrix();
}
