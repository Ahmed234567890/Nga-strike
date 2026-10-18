#include "world.h"

// ---- helpers (opaque-first rendering to avoid sort flicker) ----
static Color MixCol(Color a, Color b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
}

static Color FogShade(Color base, float dist) {
    // aerial perspective toward horizon color, opaque (no blending bugs)
    float f = (dist - 42.0f) / 65.0f;
    if (f < 0) f = 0;
    if (f > 0.75f) f = 0.75f;
    Color sky = { 170, 200, 225, 255 };
    return MixCol(base, sky, f);
}

static Color LightTop(Color base) {
    return (Color){
        (unsigned char)fminf(255, base.r * 1.14f + 10),
        (unsigned char)fminf(255, base.g * 1.14f + 10),
        (unsigned char)fminf(255, base.b * 1.14f + 8),
        255
    };
}

void World_Init(Box* boxes, int* count) {
    Color colConcrete  = (Color){ 120, 115, 105, 255 };
    Color colConcrete2 = (Color){ 140, 135, 125, 255 };
    Color colWood      = (Color){ 130,  80,  50, 255 };
    Color colWoodDark  = (Color){  70,  40,  22, 255 };
    Color colWoodCrate = (Color){ 120,  80,  50, 255 };
    Color colPillar    = (Color){ 110, 100,  90, 255 };
    Color colPlatform  = (Color){ 100,  95,  85, 255 };
    Color colStair     = (Color){  90,  85,  75, 255 };
    Color colRailing   = (Color){  60,  58,  54, 255 };

    Box b[] = {
        { {   0, 3, -50 }, { 50, 3, 0.5 }, colConcrete },
        { {   0, 3,  50 }, { 50, 3, 0.5 }, colConcrete },
        { {  50, 3,   0 }, { 0.5, 3, 50 }, colConcrete },
        { { -50, 3,   0 }, { 0.5, 3, 50 }, colConcrete },

        { { -29, 2.5, -19 }, { 2.0, 2.5, 0.3 }, colWood },
        { { -21, 2.5, -19 }, { 2.0, 2.5, 0.3 }, colWood },
        { { -25,    0.75, -31 }, { 6.0,  0.75, 0.3 }, colWood },
        { { -25,    4.0,  -31 }, { 6.0,  1.0,  0.3 }, colWood },
        { { -29.75, 2.25, -31 }, { 1.25, 0.75, 0.3 }, colWood },
        { { -25,    2.25, -31 }, { 2.0,  0.75, 0.3 }, colWood },
        { { -20.25, 2.25, -31 }, { 1.25, 0.75, 0.3 }, colWood },
        { { -31, 0.75, -25 }, { 0.3, 0.75, 6.0 }, colWood },
        { { -31, 4.0,  -25 }, { 0.3, 1.0,  6.0 }, colWood },
        { { -31, 2.25, -30.5 }, { 0.3, 0.75, 0.5 }, colWood },
        { { -31, 2.25, -25   }, { 0.3, 0.75, 3.0 }, colWood },
        { { -31, 2.25, -19.5 }, { 0.3, 0.75, 0.5 }, colWood },
        { { -19, 0.75, -25 }, { 0.3, 0.75, 6.0 }, colWood },
        { { -19, 4.0,  -25 }, { 0.3, 1.0,  6.0 }, colWood },
        { { -19, 2.25, -30.5 }, { 0.3, 0.75, 0.5 }, colWood },
        { { -19, 2.25, -25   }, { 0.3, 0.75, 3.0 }, colWood },
        { { -19, 2.25, -19.5 }, { 0.3, 0.75, 0.5 }, colWood },
        { { -25, 5.3, -25 }, { 6.5, 0.3, 6.5 }, colWoodDark },

        { {  22, 2.5, 20 }, { 1.5, 2.5, 0.3 }, colWood },
        { {  28, 2.5, 20 }, { 1.5, 2.5, 0.3 }, colWood },
        { {  25, 0.75, 30 }, { 5.0, 0.75, 0.3 }, colWood },
        { {  25, 4.0,  30 }, { 5.0, 1.0,  0.3 }, colWood },
        { {  22, 2.25, 30 }, { 2.0, 0.75, 0.3 }, colWood },
        { {  28, 2.25, 30 }, { 2.0, 0.75, 0.3 }, colWood },
        { {  20, 0.75, 25 }, { 0.3, 0.75, 5.0 }, colWood },
        { {  20, 4.0,  25 }, { 0.3, 1.0,  5.0 }, colWood },
        { {  20, 2.25, 22 }, { 0.3, 0.75, 2.0 }, colWood },
        { {  20, 2.25, 28 }, { 0.3, 0.75, 2.0 }, colWood },
        { {  30, 0.75, 25 }, { 0.3, 0.75, 5.0 }, colWood },
        { {  30, 4.0,  25 }, { 0.3, 1.0,  5.0 }, colWood },
        { {  30, 2.25, 22 }, { 0.3, 0.75, 2.0 }, colWood },
        { {  30, 2.25, 28 }, { 0.3, 0.75, 2.0 }, colWood },
        { {  25, 5.3, 25 }, { 5.5, 0.3, 5.5 }, colWoodDark },

        { { -30, 2.0, 22 }, { 3.0, 2.0, 0.3 }, colWood },
        { { -30, 2.0, 28 }, { 3.0, 2.0, 3.0 - 2.7 }, colWood },
        { { -33, 2.0, 25 }, { 0.3, 2.0, 3.0 }, colWood },
        { { -27, 2.0, 25 }, { 0.3, 2.0, 3.0 }, colWood },
        { { -30, 4.3, 25 }, { 3.5, 0.3, 3.5 }, colWoodDark },

        { { 36.5, 2.85, 25 }, { 4.5, 0.15, 4.0 }, colPlatform },
        { { 36.5, 3.3, 21 }, { 4.5, 0.3, 0.1 }, colRailing },
        { { 36.5, 3.3, 29 }, { 4.5, 0.3, 0.1 }, colRailing },
        { {  41,  3.3, 25 }, { 0.1, 0.3, 4.0 }, colRailing },
        { { 29.5, 0.25, 25 }, { 0.5, 0.25, 2.5 }, colStair },
        { { 30.0, 0.75, 25 }, { 0.5, 0.25, 2.5 }, colStair },
        { { 30.5, 1.25, 25 }, { 0.5, 0.25, 2.5 }, colStair },
        { { 31.0, 1.75, 25 }, { 0.5, 0.25, 2.5 }, colStair },
        { { 31.5, 2.25, 25 }, { 0.5, 0.25, 2.5 }, colStair },
        { { 32.0, 2.75, 25 }, { 1.0, 0.25, 2.5 }, colStair },

        { { -10, 1.5,  10 }, { 4.0, 1.5, 0.3 }, colConcrete2 },
        { {  10, 1.5, -10 }, { 4.0, 1.5, 0.3 }, colConcrete2 },
        { { -10, 1.5, -10 }, { 0.3, 1.5, 4.0 }, colConcrete2 },
        { {  10, 1.5,  10 }, { 0.3, 1.5, 4.0 }, colConcrete2 },

        { { -35, 0.75,  15 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { { -33, 0.75,  15 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { { -35, 2.25,  15 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { {  35, 0.75, -15 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { {  37, 0.75, -15 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { {   0, 0.75,  30 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { {   0, 0.75, -30 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { { -20, 0.75,   5 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { {  20, 0.75,  -5 }, { 0.75, 0.75, 0.75 }, colWoodCrate },

        { { -40, 1, -40 }, { 1.5, 1.0, 1.5 }, colPillar },
        { {  40, 1,  40 }, { 1.5, 1.0, 1.5 }, colPillar },
        { { -40, 1,  40 }, { 1.5, 1.0, 1.5 }, colPillar },
        { {  40, 1, -40 }, { 1.5, 1.0, 1.5 }, colPillar },

        // --- v3 map: south outpost (raised plaza + steps) ---
        { {   0, 0.25, -40 }, { 4.0, 0.25, 4.0 }, colPlatform },
        { {   0, 0.85, -43.7 }, { 4.0, 0.55, 0.15 }, colRailing },
        { { -3.85, 0.85, -40 }, { 0.15, 0.55, 4.0 }, colRailing },
        { {  3.85, 0.85, -40 }, { 0.15, 0.55, 4.0 }, colRailing },
        { {   0, 0.125, -35.3 }, { 2.5, 0.125, 0.5 }, colStair },
        { {   2.4, 1.25, -41.2 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { {  -2.4, 1.25, -38.6 }, { 0.75, 0.75, 0.75 }, colWoodCrate },

        // --- shipping containers (landmarks + big cover) ---
        { { -40, 1.35, 8 }, { 1.3, 1.35, 3.4 }, (Color){ 46, 86, 120, 255 } },
        { { 40.5, 1.35, -34 }, { 3.4, 1.35, 1.3 }, (Color){ 128, 52, 44, 255 } },

        // --- dumpster + pallets + concrete cover ---
        { { -22, 0.7, 18 }, { 1.1, 0.7, 0.5 }, (Color){ 52, 78, 60, 255 } },
        { { -22, 1.42, 18 }, { 1.15, 0.06, 0.55 }, (Color){ 40, 60, 46, 255 } },
        { { 18, 0.10, -20 }, { 1.1, 0.10, 0.9 }, colWoodDark },
        { { 18, 0.30, -20 }, { 1.0, 0.10, 0.85 }, colWood },
        { { 18, 0.50, -20 }, { 0.95, 0.10, 0.8 }, colWoodDark },
        { { 28, 0.9, 4 }, { 2.4, 0.9, 0.25 }, colConcrete2 },
        { { 30.15, 0.9, 6 }, { 0.25, 0.9, 2.25 }, colConcrete2 },

        // --- sandbag line near center-south ---
        { { 0, 0.35, 20 }, { 2.6, 0.35, 0.5 }, (Color){ 122, 110, 82, 255 } },
        { { 0.8, 0.89, 20.4 }, { 1.9, 0.24, 0.45 }, (Color){ 112, 101, 76, 255 } },

        // --- extra crate stack east ---
        { { 31, 0.75, -8 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { { 33, 0.75, -8 }, { 0.75, 0.75, 0.75 }, colWoodCrate },
        { { 32, 2.25, -8 }, { 0.75, 0.75, 0.75 }, colWoodCrate },

        // --- accent light fixtures (cool colored floods) ---
        { { -14, 3.1, -49.36 }, { 1.5, 0.55, 0.14 }, (Color){ 150, 232, 255, 255 } },
        { { -30, 3.2, 21.55 }, { 1.4, 0.5, 0.14 }, (Color){ 190, 182, 255, 255 } },
    };
    int n = (int)(sizeof(b) / sizeof(b[0]));
    if (n > WORLD_MAX_BOXES) n = WORLD_MAX_BOXES;
    for (int i = 0; i < n; i++) boxes[i] = b[i];
    *count = n;
}

bool World_MoveCollides(Vector3 pos, Vector3 half, const Box* boxes, int count) {
    for (int i = 0; i < count; i++) {
        if (AABBvsAABB(pos, half, boxes[i].center, boxes[i].halfExtents)) return true;
    }
    return false;
}

float World_RayHitBoxes(Ray ray, const Box* boxes, int count, Vector3* outPoint) {
    float nearest = 1e9f;
    bool found = false;
    for (int i = 0; i < count; i++) {
        BoundingBox bb = {
            Vector3Subtract(boxes[i].center, boxes[i].halfExtents),
            Vector3Add(boxes[i].center, boxes[i].halfExtents)
        };
        RayCollision rc = GetRayCollisionBox(ray, bb);
        if (rc.hit && rc.distance < nearest) {
            nearest = rc.distance;
            if (outPoint) *outPoint = rc.point;
            found = true;
        }
    }
    return found ? nearest : -1.0f;
}

bool World_LOS(Vector3 from, Vector3 to, const Box* boxes, int count) {
    Vector3 d = Vector3Subtract(to, from);
    float dist = Vector3Length(d);
    if (dist < 0.001f) return true;
    Ray r = { from, Vector3Scale(d, 1.0f / dist) };
    for (int i = 0; i < count; i++) {
        BoundingBox bb = {
            Vector3Subtract(boxes[i].center, boxes[i].halfExtents),
            Vector3Add(boxes[i].center, boxes[i].halfExtents)
        };
        RayCollision rc = GetRayCollisionBox(r, bb);
        if (rc.hit && rc.distance < dist - 0.35f) return false;
    }
    return true;
}

float World_GroundHeightAt(float x, float z, const Box* boxes, int count) {
    float h = 0.0f;
    Vector3 pHalf = { PLAYER_RADIUS, 0.05f, PLAYER_RADIUS };
    for (int i = 0; i < count; i++) {
        float top = boxes[i].center.y + boxes[i].halfExtents.y;
        if (top <= 0.02f || top > 6.0f) continue;
        Vector3 c = { x, top + 0.06f, z };
        if (AABBvsAABB(c, pHalf, boxes[i].center, boxes[i].halfExtents)) {
            if (top > h) h = top;
        }
    }
    return h;
}

// Opaque box with CPU sun shading. No transparent planes here (fixes
// flicker). Distance fog moved to the shadow-lit shader (per-pixel, matches
// barrels/enemies/pickups now that the whole lit group shares it).
static void DrawBoxEnhanced(Box b, Vector3 camPos) {
    (void)camPos;
    Vector3 size = { b.halfExtents.x * 2.0f, b.halfExtents.y * 2.0f, b.halfExtents.z * 2.0f };
    Color base = b.color;
    bool isCrate = (fabsf(size.x - 1.5f) < 0.1f && fabsf(size.y - 1.5f) < 0.1f);
    bool isWall = (size.y > 2.5f && (size.x < 1.2f || size.z < 1.2f));

    DrawCube(b.center, size.x, size.y, size.z, base);
    // opaque sun cap: sits PROUD (+0.025, spans +0.010..+0.040). A bottom
    // exactly coplanar with the face shimmers/vanishes when moving.
    if (size.x > 0.5f && size.z > 0.5f) {
        Vector3 cap = { b.center.x, b.center.y + b.halfExtents.y + 0.025f, b.center.z };
        DrawCube(cap, size.x * 0.98f, 0.03f, size.z * 0.98f, LightTop(b.color));
    }
    // opaque grime footing, slightly wider (1.02x) so sides never coplanar
    float bottom = b.center.y - b.halfExtents.y;
    if (bottom < 3.0f) {
        DrawCube((Vector3){ b.center.x, bottom + 0.10f, b.center.z },
                 size.x * 1.02f, 0.20f, size.z * 1.02f,
                 (Color){ 45, 38, 30, 255 });
    }
    DrawCubeWires(b.center, size.x, size.y, size.z, (Color){ 0, 0, 0, 110 });

    if (isCrate) {
        Vector3 fz = { b.center.x, b.center.y, b.center.z + b.halfExtents.z + 0.02f };
        DrawCube(fz, size.x * 0.92f, 0.11f, 0.03f, (Color){ 70, 42, 24, 255 });
        DrawCube((Vector3){ b.center.x, b.center.y, b.center.z - b.halfExtents.z - 0.02f },
                 size.x * 0.92f, 0.11f, 0.03f, (Color){ 70, 42, 24, 255 });
        for (int k = -1; k <= 1; k += 2)
            for (int j = -1; j <= 1; j += 2)
                DrawSphereEx((Vector3){ b.center.x + k * size.x * 0.38f, b.center.y + j * size.y * 0.38f,
                                        b.center.z + b.halfExtents.z + 0.02f }, 0.028f, 6, 5,
                             (Color){ 40, 26, 14, 255 });
    }
    if (isWall && size.y > 3.5f && size.x > 3.0f) {
        for (int k = -1; k <= 1; k += 2) {
            Vector3 wp = { b.center.x + k * size.x * 0.25f, b.center.y + 0.4f, b.center.z + b.halfExtents.z + 0.03f };
            DrawCube(wp, 0.92f, 0.92f, 0.04f, (Color){ 25, 35, 50, 255 });
            DrawCube((Vector3){ wp.x, wp.y, wp.z + 0.022f }, 0.78f, 0.78f, 0.02f,
                     (Color){ 150, 190, 215, 255 });
            DrawCube((Vector3){ wp.x, wp.y, wp.z + 0.032f }, 0.07f, 0.78f, 0.02f,
                     (Color){ 60, 36, 20, 255 });
            DrawCube((Vector3){ wp.x, wp.y, wp.z + 0.032f }, 0.78f, 0.07f, 0.02f,
                     (Color){ 60, 36, 20, 255 });
        }
        DrawCube((Vector3){ b.center.x, b.center.y + b.halfExtents.y - 0.12f, b.center.z },
                 size.x + 0.10f, 0.18f, size.z + 0.10f, (Color){ 60, 36, 20, 255 });
    }
}

// Smooth vertical gradient dome around the camera (unlit, opaque). Drawn
// with the default shader BEFORE the lit group: gives the sky a modern
// top-down gradient instead of a single flat clear color.
static void DrawSkyDome(Vector3 camPos) {
    const int SEG = 28;
    const float R = 350.0f;
    // elevation angle (deg) + color per ring; below-horizon ring keeps the
    // horizon color so the dome always meets the ground haze cleanly
    static const float ele[6] = { -5.0f, 3.0f, 12.0f, 24.0f, 42.0f, 62.0f };
    static const unsigned char cols[7][3] = {
        { 195, 215, 232 }, // below horizon
        { 200, 218, 236 },
        { 205, 222, 242 },
        { 188, 210, 240 }, // 24 deg
        { 150, 182, 236 }, // 42 deg
        { 108, 148, 228 }, // 62 deg
        {  72, 112, 208 }, // zenith
    };
    rlBegin(RL_TRIANGLES);
    for (int r = 0; r < 5; r++) {
        float e0 = ele[r] * DEG2RAD, e1 = ele[r + 1] * DEG2RAD;
        float r0 = R * cosf(e0), y0 = R * sinf(e0);
        float r1 = R * cosf(e1), y1 = R * sinf(e1);
        for (int s = 0; s < SEG; s++) {
            float a0 = (float)s / (float)SEG * 2.0f * PI;
            float a1 = (float)(s + 1) / (float)SEG * 2.0f * PI;
            float x00 = camPos.x + r0 * cosf(a0), z00 = camPos.z + r0 * sinf(a0);
            float x01 = camPos.x + r0 * cosf(a1), z01 = camPos.z + r0 * sinf(a1);
            float x10 = camPos.x + r1 * cosf(a0), z10 = camPos.z + r1 * sinf(a0);
            float x11 = camPos.x + r1 * cosf(a1), z11 = camPos.z + r1 * sinf(a1);
            rlColor4ub(cols[r][0], cols[r][1], cols[r][2], 255);
            rlVertex3f(x00, y0, z00);
            rlColor4ub(cols[r][0], cols[r][1], cols[r][2], 255);
            rlVertex3f(x01, y0, z01);
            rlColor4ub(cols[r + 1][0], cols[r + 1][1], cols[r + 1][2], 255);
            rlVertex3f(x10, y1, z10);
            rlColor4ub(cols[r + 1][0], cols[r + 1][1], cols[r + 1][2], 255);
            rlVertex3f(x11, y1, z11);
            rlColor4ub(cols[r + 1][0], cols[r + 1][1], cols[r + 1][2], 255);
            rlVertex3f(x10, y1, z10);
            rlColor4ub(cols[r][0], cols[r][1], cols[r][2], 255);
            rlVertex3f(x01, y0, z01);
        }
    }
    // zenith cap
    for (int s = 0; s < SEG; s++) {
        float a0 = (float)s / (float)SEG * 2.0f * PI;
        float a1 = (float)(s + 1) / (float)SEG * 2.0f * PI;
        float e1 = ele[5] * DEG2RAD;
        float r1 = R * cosf(e1), y1 = R * sinf(e1);
        rlColor4ub(cols[6][0], cols[6][1], cols[6][2], 255);
        rlVertex3f(camPos.x, R, camPos.z);
        rlColor4ub(cols[5][0], cols[5][1], cols[5][2], 255);
        rlVertex3f(camPos.x + r1 * cosf(a0), y1, camPos.z + r1 * sinf(a0));
        rlVertex3f(camPos.x + r1 * cosf(a1), y1, camPos.z + r1 * sinf(a1));
    }
    rlEnd();
}

void World_DrawSky(float time, Vector3 camPos) {
    DrawSkyDome(camPos);
    // Opaque sun + mountains (no alpha sorting bugs)
    DrawSphereEx((Vector3){ 90, 70, -90 }, 9.0f, 16, 12, (Color){ 255, 244, 210, 255 });
    Color mcol = FogShade((Color){ 100, 116, 142, 255 }, 110.0f);
    Color mhi = FogShade((Color){ 160, 175, 195, 255 }, 110.0f);
    DrawCube((Vector3){ -75, 6, -60 }, 30, 14, 8, mcol);
    DrawCube((Vector3){ -75, 9.5f, -60 }, 30, 1.2f, 8.2f, mhi);
    DrawCube((Vector3){ 70, 5, -70 }, 36, 12, 10, mcol);
    DrawCube((Vector3){ 70, 8.2f, -70 }, 36, 1.0f, 10.2f, mhi);
    DrawCube((Vector3){ -70, 5, 70 }, 34, 11, 9, mcol);
    DrawCube((Vector3){ 75, 6, 65 }, 28, 13, 8, mcol);
    // Opaque horizon band (replaces transparent haze cubes)
    DrawCube((Vector3){ 0, 2.2f, -68 }, 150, 4.5f, 1.2f, (Color){ 195, 215, 232, 255 });
    DrawCube((Vector3){ 0, 2.2f, 68 }, 150, 4.5f, 1.2f, (Color){ 195, 215, 232, 255 });
    DrawCube((Vector3){ -68, 2.2f, 0 }, 1.2f, 4.5f, 150, (Color){ 195, 215, 232, 255 });
    DrawCube((Vector3){ 68, 2.2f, 0 }, 1.2f, 4.5f, 150, (Color){ 195, 215, 232, 255 });
    // Opaque clouds (fixes transparent sphere sorting)
    for (int i = 0; i < 8; i++) {
        float sp = 0.5f + 0.16f * (float)i;
        float x = fmodf((float)i * 31.0f + time * sp * 2.2f, 170.0f) - 85.0f;
        float z = -60.0f + (float)(i % 4) * 38.0f;
        float y = 25.0f + (float)(i % 4) * 3.2f;
        Color c = { 252, 253, 255, 255 };
        Color sh = { 218, 226, 236, 255 };
        DrawSphereEx((Vector3){ x, y - 0.55f, z }, 3.3f, 12, 8, sh);
        DrawSphereEx((Vector3){ x, y, z }, 3.2f, 12, 8, c);
        DrawSphereEx((Vector3){ x + 3.1f, y - 0.4f, z }, 2.3f, 10, 8, c);
        DrawSphereEx((Vector3){ x - 3.1f, y - 0.5f, z }, 2.0f, 10, 8, c);
    }
    for (int i = 0; i < 5; i++) {
        float bx = fmodf((float)i * 53.0f + time * (3.0f + i), 140.0f) - 70.0f;
        float by = 20.0f + sinf(time * 0.7f + i * 1.7f) * 1.5f;
        float bz = -40.0f + (i % 3) * 22.0f;
        float flap = sinf(time * 9.0f + i * 2.0f) * 0.35f;
        Vector3 p = { bx, by, bz };
        DrawLine3D((Vector3){ p.x - 0.6f, p.y + flap, p.z }, p, BLACK);
        DrawLine3D(p, (Vector3){ p.x + 0.6f, p.y + flap, p.z }, BLACK);
    }
}

// Layered heights: ground 0 -> patches 0.01 -> roads 0.06 top -> dashes 0.08
// -> shadows 0.10 -> decals 0.12. No coplanar planes.
static void DrawProps(float time, Vector3 camPos) {
    (void)camPos;
    // roads as thin opaque boxes (no z-fight)
    DrawCube((Vector3){ 0, 0.03f, 0 }, 96, 0.06f, 7, (Color){ 60, 60, 64, 255 });
    DrawCube((Vector3){ 0, 0.03f, 0 }, 7, 0.06f, 96, (Color){ 60, 60, 64, 255 });
    DrawCube((Vector3){ 0, 0.065f, 0 }, 0.6f, 0.015f, 0.6f, (Color){ 70, 70, 74, 255 });
    for (int k = -6; k <= 6; k++) {
        if (k == 0) continue;
        DrawCube((Vector3){ (float)k * 7.0f, 0.068f, 0 }, 2.6f, 0.015f, 0.30f, (Color){ 232, 202, 82, 255 });
        DrawCube((Vector3){ 0, 0.068f, (float)k * 7.0f }, 0.30f, 0.015f, 2.6f, (Color){ 232, 202, 82, 255 });
    }
    DrawCube((Vector3){ 0, 0.05f, 4.2f }, 96, 0.04f, 0.9f, (Color){ 172, 170, 162, 255 });
    DrawCube((Vector3){ 0, 0.05f, -4.2f }, 96, 0.04f, 0.9f, (Color){ 172, 170, 162, 255 });
    DrawCube((Vector3){ 4.2f, 0.05f, 0 }, 0.9f, 0.04f, 96, (Color){ 172, 170, 162, 255 });
    DrawCube((Vector3){ -4.2f, 0.05f, 0 }, 0.9f, 0.04f, 96, (Color){ 172, 170, 162, 255 });

    Vector3 lamps[4] = { { -12, 0, -12 }, { 12, 0, 12 }, { -12, 0, 14 }, { 14, 0, -12 } };
    for (int i = 0; i < 4; i++) {
        Vector3 base = lamps[i];
        DrawCylinderEx(base, (Vector3){ base.x, 0.15f, base.z }, 0.30f, 0.34f, 8, (Color){ 50, 50, 55, 255 });
        DrawCylinderEx((Vector3){ base.x, 0.12f, base.z }, (Vector3){ base.x, 4.6f, base.z }, 0.09f, 0.09f, 8, (Color){ 45, 46, 50, 255 });
        DrawCube((Vector3){ base.x, 4.7f, base.z }, 0.72f, 0.18f, 0.36f, (Color){ 40, 41, 45, 255 });
        Vector3 bulb = { base.x + 0.2f, 4.55f, base.z };
        DrawSphereEx(bulb, 0.14f, 10, 8, (Color){ 255, 250, 220, 255 });
    }

    static Vector3 trees[10];
    static bool tinit = false;
    if (!tinit) {
        tinit = true;
        Vector3 seed[10] = { { -44, 0, -8 }, { -44, 0, 8 }, { 44, 0, 6 }, { 44, 0, -8 },
                             { -8, 0, -44 }, { 10, 0, -44 }, { -12, 0, 44 }, { 8, 0, 44 },
                             { -44, 0, -34 }, { 44, 0, 36 } };
        for (int i = 0; i < 10; i++) trees[i] = seed[i];
    }
    for (int i = 0; i < 10; i++) {
        Vector3 t = trees[i];
        float sway = sinf(time * 1.1f + (float)i * 1.3f) * 0.07f;
        DrawCylinderEx((Vector3){ t.x, 0, t.z }, (Vector3){ t.x, 0.14f, t.z }, 0.62f, 0.68f, 8, (Color){ 60, 50, 40, 255 });
        DrawCylinderEx((Vector3){ t.x, 0.1f, t.z }, (Vector3){ t.x, 2.1f, t.z }, 0.20f, 0.30f, 7, (Color){ 92, 62, 40, 255 });
        DrawCylinderEx((Vector3){ t.x, 2.0f, t.z }, (Vector3){ t.x + 0.7f, 2.9f, t.z + 0.3f }, 0.10f, 0.14f, 6, (Color){ 92, 62, 40, 255 });
        Color leaf = (Color){ 54 + (i % 3) * 9, 112 + (i % 2) * 12, 54, 255 };
        Color leaf2 = (Color){ 42, 92, 44, 255 };
        Color leafHi = (Color){ 96, 155, 84, 255 };
        DrawSphereEx((Vector3){ t.x + sway, 3.0f, t.z }, 1.55f, 12, 8, leaf);
        DrawSphereEx((Vector3){ t.x + 0.9f + sway, 2.4f, t.z + 0.4f }, 1.05f, 10, 8, leaf2);
        DrawSphereEx((Vector3){ t.x - 0.8f + sway, 2.5f, t.z - 0.3f }, 0.95f, 10, 8, leaf);
        DrawSphereEx((Vector3){ t.x + sway * 1.4f, 3.75f, t.z }, 0.8f, 10, 8, leafHi);
    }
    static Vector3 rocks[8];
    static bool rinit = false;
    if (!rinit) {
        rinit = true;
        for (int i = 0; i < 8; i++) rocks[i] = (Vector3){ RandRange(-46, 46), 0, RandRange(-46, 46) };
    }
    for (int i = 0; i < 8; i++) {
        Vector3 r = rocks[i];
        if (fabsf(r.x) < 6 || fabsf(r.z) < 6) continue;
        float s = 0.5f + (i % 3) * 0.15f;
        DrawSphereEx((Vector3){ r.x, s * 0.35f, r.z }, s, 9, 6, (Color){ 126, 123, 116, 255 });
        DrawSphereEx((Vector3){ r.x + 0.32f, 0.16f, r.z + 0.2f }, 0.30f, 8, 6, (Color){ 146, 143, 136, 255 });
        DrawSphereEx((Vector3){ r.x - 0.2f, s * 0.72f, r.z }, s * 0.45f, 8, 6, (Color){ 155, 152, 145, 255 });
    }
    static Vector3 grass[150];
    static bool ginit = false;
    if (!ginit) {
        ginit = true;
        for (int i = 0; i < 150; i++) grass[i] = (Vector3){ RandRange(-47, 47), 0.16f, RandRange(-47, 47) };
    }
    for (int i = 0; i < 150; i++) {
        if (fabsf(grass[i].x) < 4.6f || fabsf(grass[i].z) < 4.6f) continue;
        float s = 0.10f + (i % 4) * 0.03f;
        Color gc = (i & 1) ? (Color){ 88, 142, 72, 255 } : (Color){ 112, 157, 82, 255 };
        DrawCubeV(grass[i], (Vector3){ s, 0.24f, s }, gc);
        DrawCubeV((Vector3){ grass[i].x, grass[i].y + 0.14f, grass[i].z }, (Vector3){ s * 0.55f, 0.10f, s * 0.55f },
                  (Color){ 142, 182, 102, 255 });
    }
}

void World_DrawLit(const Box* boxes, int count, float time, Vector3 camPos) {
    // Opaque ground first. NOTE: DrawGrid draws at y=0, so ground must sit
    // BELOW it (y=-0.02) or the two coplanar surfaces z-fight and flicker
    // whenever the camera moves.
    DrawPlane((Vector3){ 0, -0.02f, 0 }, (Vector2){ GROUND_SIZE, GROUND_SIZE }, (Color){ 178, 160, 122, 255 });
    // Large soft variation (few big quads = no moire shimmer)
    DrawPlane((Vector3){ -20, -0.012f, -10 }, (Vector2){ 34, 24 }, (Color){ 168, 148, 112, 255 });
    DrawPlane((Vector3){ 22, -0.012f, 18 }, (Vector2){ 28, 22 }, (Color){ 172, 160, 118, 255 });
    DrawPlane((Vector3){ 5, -0.011f, -30 }, (Vector2){ 42, 17 }, (Color){ 165, 148, 112, 255 });
    DrawProps(time, camPos);
    static bool init = false;
    static Vector3 peb[110];
    static float pebS[110];
    if (!init) {
        init = true;
        for (int i = 0; i < 110; i++) {
            peb[i] = (Vector3){ RandRange(-47, 47), 0.09f, RandRange(-47, 47) };
            pebS[i] = RandRange(0.06f, 0.17f);
        }
    }
    for (int i = 0; i < 110; i++) {
        if (fabsf(peb[i].x) < 4.2f || fabsf(peb[i].z) < 4.2f) continue;
        DrawCubeV(peb[i], (Vector3){ pebS[i], 0.07f, pebS[i] }, (Color){ 122, 107, 87, 255 });
    }
    for (int i = 0; i < count; i++) DrawBoxEnhanced(boxes[i], camPos);
}

// Grid lines carry no reliable normals: keep them on the default shader.
void World_DrawGrid(void) {
    DrawGrid(60, 2.0f);
}
