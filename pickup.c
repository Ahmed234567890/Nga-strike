#include "pickup.h"
#include "fx.h"

void Pickup_Init(Pickup* pickups, int count) {
    Pickup init[MAX_PICKUPS] = {
        { { -30, 1.0f,  -5 }, PICKUP_AMMO,   true, 0, 0 },
        { {  30, 1.0f,   5 }, PICKUP_AMMO,   true, 0, 0 },
        // NOTE: keep clear of spawn (0,0): a cube in the player's face clips
        // the near plane and strobes fullscreen whenever you turn/move.
        { {   6, 1.0f,   9 }, PICKUP_AMMO,   true, 0, 0 },
        { { -25, 1.0f, -25 }, PICKUP_AMMO,   true, 0, 0 },
        { {  25, 1.0f,  25 }, PICKUP_HEALTH, true, 0, 0 },
        { { -30, 1.0f,  25 }, PICKUP_HEALTH, true, 0, 0 },
        { {  36.5f, 4.5f, 25 }, PICKUP_HEALTH, true, 0, 0 },
        { {   0, 1.0f,  35 }, PICKUP_AMMO,   true, 0, 0 },
    };
    int n = count < MAX_PICKUPS ? count : MAX_PICKUPS;
    for (int i = 0; i < n; i++) pickups[i] = init[i];
}

void Pickup_Update(Pickup* pickups, int count, Player* player, Weapon* weapons, int wc,
                   Audio* audio, float dt, Particle* fx, int fxCount) {
    for (int i = 0; i < count; i++) {
        if (!pickups[i].active) {
            pickups[i].respawnTimer -= dt;
            if (pickups[i].respawnTimer <= 0) { pickups[i].active = true; pickups[i].bobTimer = 0; }
            continue;
        }
        pickups[i].bobTimer += dt;
        float dx = player->position.x - pickups[i].position.x;
        float dz = player->position.z - pickups[i].position.z;
        float dy = (player->position.y + 1.0f) - pickups[i].position.y;
        if (dx*dx + dy*dy + dz*dz > PICKUP_RADIUS * PICKUP_RADIUS) continue;

        bool picked = false;
        if (pickups[i].type == PICKUP_AMMO) {
            bool need = false;
            for (int k = 0; k < wc; k++) if (weapons[k].ammoReserve < weapons[k].reserveMax) { need = true; break; }
            if (need) {
                for (int k = 0; k < wc; k++) {
                    weapons[k].ammoReserve += PICKUP_AMMO_AMOUNT;
                    if (weapons[k].ammoReserve > weapons[k].reserveMax) weapons[k].ammoReserve = weapons[k].reserveMax;
                }
                picked = true;
            }
        } else if (player->health < PLAYER_MAX_HEALTH) {
            player->health += PICKUP_HEALTH_AMOUNT;
            if (player->health > PLAYER_MAX_HEALTH) player->health = PLAYER_MAX_HEALTH;
            picked = true;
        }
        if (picked) {
            pickups[i].active = false;
            pickups[i].respawnTimer = PICKUP_RESPAWN_TIME;
            Color c = (pickups[i].type == PICKUP_AMMO) ? (Color){ 240, 200, 80, 255 } : (Color){ 120, 255, 120, 255 };
            if (fx) FX_SpawnPickupBurst(fx, fxCount, pickups[i].position, c);
            Audio_PlayPickup(audio, 1.1f + RandomFloat() * 0.25f);
        }
    }
}

void Pickup_Draw(Pickup* pickups, int count, Vector3 camPos) {
    for (int i = 0; i < count; i++) {
        if (!pickups[i].active) continue;
        // Near-plane cull: closer than this the cube sweeps fullscreen and
        // strobes through the near plane while turning/walking.
        Vector3 d = Vector3Subtract(pickups[i].position, camPos);
        if (Vector3LengthSqr(d) < 0.81f) continue;
        float bob = sinf(pickups[i].bobTimer * 2.6f) * 0.15f;
        Vector3 pos = { pickups[i].position.x, pickups[i].position.y + bob, pickups[i].position.z };
        // opaque base disc (no transparent sorting) + raised shadow layer
        Color ring = (pickups[i].type == PICKUP_AMMO) ? (Color){ 200, 165, 45, 255 } : (Color){ 200, 80, 80, 255 };
        DrawCylinder((Vector3){ pos.x, 0.10f, pos.z }, 0.55f, 0.55f, 0.03f, 12, (Color){ 45, 42, 38, 255 });
        DrawCylinder((Vector3){ pos.x, 0.125f, pos.z }, 0.48f, 0.48f, 0.02f, 12, ring);

        rlPushMatrix();
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef(pickups[i].bobTimer * 45.0f, 0, 1, 0);
        if (pickups[i].type == PICKUP_AMMO) {
            DrawCubeV((Vector3){ 0 }, (Vector3){ 0.55f, 0.55f, 0.55f }, (Color){ 200, 165, 45, 255 });
            DrawCubeWiresV((Vector3){ 0 }, (Vector3){ 0.55f, 0.55f, 0.55f }, (Color){ 120, 95, 20, 255 });
            // bands sit PROUD (±0.29 vs body ±0.275): interpenetrating bands
            // z-fight and shimmer at any distance.
            DrawCubeV((Vector3){ 0, 0.29f, 0 }, (Vector3){ 0.58f, 0.03f, 0.58f }, (Color){ 240, 220, 120, 255 });
            DrawCubeV((Vector3){ 0, -0.29f, 0 }, (Vector3){ 0.58f, 0.03f, 0.58f }, (Color){ 240, 220, 120, 255 });
        } else {
            DrawCubeV((Vector3){ 0 }, (Vector3){ 0.55f, 0.55f, 0.55f }, (Color){ 210, 55, 55, 255 });
            DrawCubeWiresV((Vector3){ 0 }, (Vector3){ 0.55f, 0.55f, 0.55f }, (Color){ 120, 25, 25, 255 });
            DrawCubeV((Vector3){ 0, 0.28f, 0 }, (Vector3){ 0.36f, 0.02f, 0.12f }, WHITE);
            DrawCubeV((Vector3){ 0, 0.28f, 0 }, (Vector3){ 0.12f, 0.02f, 0.36f }, WHITE);
        }
        rlPopMatrix();
    }
}
