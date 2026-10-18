#include "weapon.h"

void Weapon_Init(Weapon* w, int count) {
    Weapon defs[WEAPON_COUNT] = {
        // semi-auto: one click = one shot (autoFire=false)
        { "PISTOL", 12, 12, 96, 96, 0.16f, 1.2f, 34.0f, 95.0f,
          0.012f, 0.009f, 0.07f, 55.0f, 0.75f, 0.012f, 0.003f, 90.0f, false, false },
        { "AKR", 30, 30, 180, 180, 0.105f, 2.0f, 26.0f, 78.0f,
          0.016f, 0.010f, 0.085f, 48.0f, 0.55f, 0.022f, 0.008f, 100.0f, true, false },
        { "SNIPER", 5, 5, 25, 25, 1.25f, 2.6f, 120.0f, 400.0f,
          0.05f, 0.018f, 0.22f, 18.0f, 0.25f, 0.03f, 0.0008f, 220.0f, false, true },
    };
    int n = count < WEAPON_COUNT ? count : WEAPON_COUNT;
    for (int i = 0; i < n; i++) w[i] = defs[i];
}

bool Weapon_ReloadInsertCrossed(float prevT, float newT) {
    return prevT < 0.62f && newT >= 0.62f;
}

float Weapon_ReloadTime(const Weapon* w, bool empty) {
    return empty ? w->reloadTime * 1.25f : w->reloadTime;
}

static float Smooth(float t) { t = ClampF(t, 0, 1); return t * t * (3 - 2 * t); }
static float Phase(float t, float a, float b) { return Smooth((t - a) / (b - a)); }

static Vector3 ApplyAds(Vector3 off, float adsBlend) {
    off.x -= 0.24f * adsBlend;
    off.y += 0.10f * adsBlend;
    return off;
}

// Animation clock for idle micro-motion (set once per frame by Weapon_DrawViewmodel).
static float s_animTime = 0.0f;

// Keyframed support-hand path in gun-local space; smoothstep between keys,
// plus a tiny per-segment bulge so hand moves read springy instead of linear.
typedef struct { float t; Vector3 p; } HandKey;
static Vector3 HandPath(const HandKey* k, int n, float t) {
    if (t <= k[0].t) return k[0].p;
    for (int i = 0; i < n - 1; i++) {
        if (t <= k[i + 1].t) {
            float f = ClampF((t - k[i].t) / (k[i + 1].t - k[i].t), 0.0f, 1.0f);
            Vector3 d = Vector3Subtract(k[i + 1].p, k[i].p);
            float len = Vector3Length(d);
            Vector3 p = Vector3Lerp(k[i].p, k[i + 1].p, Smooth(f));
            if (len > 0.05f)
                p = Vector3Add(p, Vector3Scale(d, sinf(f * PI) * 0.055f / len));
            return p;
        }
    }
    return k[n - 1].p;
}

// Magazine seating jolt: tiny weapon dip + roll kick as the fresh mag locks
// in (fires once per reload around the seat keyframe).
static void SeatJolt(float reloadT, float seatT, Vector3* off, float* roll, float* pitchTilt) {
    if (reloadT <= 0.0f) return;
    float d = reloadT - seatT;
    float j = expf(-d * d * 150.0f);
    off->y -= 0.024f * j;
    *roll += 4.0f * j;
    *pitchTilt += -2.5f * j;
}

// tilt the whole gun during reload: dip + roll + yaw (kept small to stay in frustum)
static void ReloadTilt(float reloadT, Vector3* off, float* roll, float* pitchTilt) {
    if (reloadT <= 0) { *roll = 0; *pitchTilt = 0; return; }
    float dip = sinf(ClampF(reloadT, 0, 1) * PI);
    off->y -= dip * 0.15f;
    off->x += dip * 0.04f;
    off->z += dip * 0.03f;
    // roll gun inward mid-reload so mag well faces viewer
    *roll = sinf(ClampF(reloadT, 0, 1) * PI) * 17.0f;
    *pitchTilt = sinf(ClampF(reloadT, 0, 1) * PI) * -8.0f;
}

static void DrawPistolViewmodel(Vector3 off, float muzzleTimer, float reloadT) {
    Color skin      = (Color){ 228, 178, 138, 255 };
    Color skinLine  = (Color){ 110,  75,  55, 255 };
    Color gunSlide  = (Color){  62,  65,  72, 255 };
    Color gunFrame  = (Color){  34,  36,  40, 255 };
    Color gunDark   = (Color){  16,  18,  22, 255 };
    Color gunSteel  = (Color){  95,  98, 105, 255 };
    Color gunSerra  = (Color){  24,  26,  30, 255 };
    Color brass     = (Color){ 232, 190, 90, 255 };

    float roll = 0, ptilt = 0;
    ReloadTilt(reloadT, &off, &roll, &ptilt);
    SeatJolt(reloadT, 0.705f, &off, &roll, &ptilt);

    // staged mag: old drops 0.12-0.45, new rises 0.45-0.68
    float oldMag = 1.0f - Phase(reloadT, 0.12f, 0.45f);
    float newMag = Phase(reloadT, 0.45f, 0.68f);
    // slide rack 0.72-0.92 (back then snap)
    float rack = 0;
    if (reloadT > 0.72f && reloadT < 0.92f) rack = sinf((reloadT - 0.72f) / 0.20f * PI);
    // LEFT hand: cupped under the right at rest; runs mag + slide during reload
    static const HandKey keys[] = {
        { 0.00f, { 0.16f, -0.56f, -0.86f } },  // two-hand support grip
        { 0.14f, { 0.32f, -0.80f, -0.90f } },  // grab mag base
        { 0.30f, { 0.33f, -0.95f, -0.90f } },  // pull old mag out
        { 0.45f, { 0.27f, -1.08f, -0.80f } },  // release it low
        { 0.55f, { -0.10f, -1.16f, -0.55f } }, // reach belt (off-screen)
        { 0.68f, { 0.32f, -0.92f, -0.90f } },  // bring fresh mag to well
        { 0.75f, { 0.32f, -0.82f, -0.90f } },  // seat it
        { 0.78f, { 0.30f, -0.36f, -0.97f } },  // grab slide serrations
        { 0.83f, { 0.30f, -0.36f, -0.88f } },  // rack back
        { 0.92f, { 0.30f, -0.36f, -0.97f } },  // release slide
        { 1.00f, { 0.16f, -0.56f, -0.86f } },  // back to support
    };
    Vector3 lHand = HandPath(keys, 11, reloadT);
    lHand.y += sinf(s_animTime * 1.7f) * 0.004f;

    rlPushMatrix();
    rlTranslatef(0.30f, -0.40f, -1.10f);
    rlRotatef(roll, 0, 0, 1);
    rlRotatef(ptilt, 1, 0, 0);
    rlTranslatef(-0.30f, 0.40f, 1.10f);

    Vector3 hp = Vector3Add((Vector3){ 0.32f, -0.50f, -0.88f }, off);
    DrawLimbBlocky(Vector3Add((Vector3){ 0.66f, -0.90f, -0.55f }, off), hp, 0.195f, skin);
    DrawCubeV(hp, (Vector3){ 0.24f, 0.24f, 0.24f }, skin);
    DrawCubeWiresV(hp, (Vector3){ 0.24f, 0.24f, 0.24f }, skinLine);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.62f, -0.90f }, off), (Vector3){ 0.12f, 0.28f, 0.14f }, gunFrame);
    DrawCubeV(Vector3Add((Vector3){ 0.30f, -0.42f, -1.00f }, off), (Vector3){ 0.11f, 0.06f, 0.30f }, gunFrame);
    Vector3 slidePos = Vector3Add((Vector3){ 0.30f, -0.34f, -1.14f }, off);
    slidePos.z += rack * 0.09f; // slide back
    DrawCubeV(slidePos, (Vector3){ 0.13f, 0.14f, 0.55f }, gunSlide);
    DrawCubeWiresV(slidePos, (Vector3){ 0.13f, 0.14f, 0.55f }, gunDark);
    // slide highlight when racked
    if (rack > 0.02f) DrawCubeV(Vector3Add(slidePos, (Vector3){ 0, 0.075f, 0 }), (Vector3){ 0.09f, 0.006f, 0.5f }, gunSteel);
    for (int i = 0; i < 3; i++)
        DrawCubeV(Vector3Add(slidePos, (Vector3){ 0, 0, 0.19f - i * 0.035f }), (Vector3){ 0.132f, 0.14f, 0.012f }, gunSerra);
    DrawCubeV(Vector3Add((Vector3){ 0.30f, -0.24f, -0.90f }, off), (Vector3){ 0.11f, 0.03f, 0.03f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.30f, -0.24f, -1.38f }, off), (Vector3){ 0.03f, 0.03f, 0.03f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.30f, -0.46f, -0.94f }, off), (Vector3){ 0.03f, 0.05f, 0.02f }, gunSteel);
    DrawCylinderEx(Vector3Add((Vector3){ 0.30f, -0.32f, -1.41f }, off),
                   Vector3Add((Vector3){ 0.30f, -0.32f, -1.47f }, off), 0.035f, 0.035f, 12, gunSteel);
    // left arm: off-screen bottom-left anchor -> working hand (inside tilt so it follows the gun)
    Vector3 lHandW = Vector3Add(lHand, off);
    DrawLimbBlocky(Vector3Add((Vector3){ -0.60f, -0.92f, -0.55f }, off), lHandW, 0.15f, skin);
    DrawCubeV(lHandW, (Vector3){ 0.21f, 0.21f, 0.21f }, skin);
    DrawCubeWiresV(lHandW, (Vector3){ 0.21f, 0.21f, 0.21f }, skinLine);
    // fresh mag rides in the left hand while it travels
    if (reloadT > 0.52f && reloadT < 0.70f)
        DrawCubeV(Vector3Add(lHandW, (Vector3){ 0, -0.13f, 0 }), (Vector3){ 0.115f, 0.16f, 0.13f }, gunDark);
    rlPopMatrix();

    // mags drawn outside tilt so drop looks physical (opaque: no alpha sort bugs)
    if (reloadT > 0.0f && reloadT < 1.0f) {
        if (oldMag > 0.15f) {
            float drop = (1.0f - oldMag);
            Vector3 mp = Vector3Add((Vector3){ 0.32f, -0.79f, -0.90f }, off);
            mp.y -= drop * drop * 0.55f;
            mp.x += drop * 0.05f;
            DrawCubeV(mp, (Vector3){ 0.115f, 0.10f, 0.13f }, brass);
        }
        if (newMag >= 0.99f)
            DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.79f, -0.90f }, off), (Vector3){ 0.115f, 0.045f, 0.13f }, gunDark);
    } else {
        DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.79f, -0.90f }, off), (Vector3){ 0.115f, 0.045f, 0.13f }, gunDark);
    }

    if (muzzleTimer > 0.0f) {
        float t = muzzleTimer / MUZZLE_FLASH_TIME;
        float s = 0.05f * (0.7f + RandomFloat() * 0.6f) * (0.4f + 0.6f * t);
        Vector3 fp = Vector3Add(off, (Vector3){ 0.30f, -0.32f, -1.52f });
        DrawSphereEx(fp, s * 1.8f, 10, 8, (Color){ 255, 200, 80, 255 });
        DrawSphereEx(fp, s * 0.8f, 8, 6, (Color){ 255, 255, 240, 255 });
        DrawCubeV(fp, (Vector3){ s * 7.0f, s * 0.5f, s * 0.5f }, (Color){ 255, 230, 150, 255 });
        DrawCubeV(fp, (Vector3){ s * 0.5f, s * 7.0f, s * 0.5f }, (Color){ 255, 230, 150, 255 });
    }
    (void)skinLine;
}

static void DrawAKRViewmodel(Vector3 off, float muzzleTimer, float reloadT) {
    Color skin     = (Color){ 228, 178, 138, 255 };
    Color gunBody  = (Color){  42,  44,  46, 255 };
    Color gunBody2 = (Color){  28,  30,  32, 255 };
    Color gunDark  = (Color){  14,  16,  18, 255 };
    Color gunSteel = (Color){  78,  82,  86, 255 };
    Color woodCol  = (Color){ 110,  68,  38, 255 };
    Color woodDark = (Color){  72,  44,  24, 255 };

    float roll = 0, ptilt = 0;
    ReloadTilt(reloadT, &off, &roll, &ptilt);
    SeatJolt(reloadT, 0.685f, &off, &roll, &ptilt);
    float oldMag = 1.0f - Phase(reloadT, 0.10f, 0.42f);
    float newMag = Phase(reloadT, 0.42f, 0.66f);
    float charge = 0;
    if (reloadT > 0.72f && reloadT < 0.94f) charge = sinf((reloadT - 0.72f) / 0.22f * PI);
    // LEFT hand: supports the handguard at rest; runs mag + charging handle during reload
    static const HandKey keys[] = {
        { 0.00f, { 0.32f, -0.50f, -1.80f } },  // support the handguard
        { 0.14f, { 0.32f, -0.72f, -1.15f } },  // grab mag
        { 0.32f, { 0.31f, -0.88f, -1.10f } },  // pull old mag out
        { 0.42f, { 0.24f, -1.04f, -0.95f } },  // release low
        { 0.52f, { -0.12f, -1.18f, -0.70f } }, // belt (off-screen)
        { 0.66f, { 0.32f, -0.80f, -1.15f } },  // fresh mag to well
        { 0.71f, { 0.32f, -0.68f, -1.15f } },  // seat it
        { 0.74f, { 0.42f, -0.38f, -1.00f } },  // reach charging handle
        { 0.83f, { 0.42f, -0.38f, -0.86f } },  // pull back
        { 0.94f, { 0.42f, -0.38f, -1.02f } },  // release
        { 1.00f, { 0.32f, -0.50f, -1.80f } },  // back to handguard
    };
    Vector3 lHand = HandPath(keys, 11, reloadT);
    lHand.y += sinf(s_animTime * 1.7f) * 0.004f;

    rlPushMatrix();
    rlTranslatef(0.32f, -0.42f, -1.28f);
    rlRotatef(roll, 0, 0, 1);
    rlRotatef(ptilt, 1, 0, 0);
    rlTranslatef(-0.32f, 0.42f, 1.28f);

    Vector3 hp = Vector3Add((Vector3){ 0.34f, -0.52f, -0.92f }, off);
    DrawLimbBlocky(Vector3Add((Vector3){ 0.72f, -0.92f, -0.60f }, off), hp, 0.195f, skin);
    DrawCubeV(hp, (Vector3){ 0.24f, 0.24f, 0.24f }, skin);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.66f, -0.94f }, off), (Vector3){ 0.13f, 0.30f, 0.15f }, woodCol);
    Vector3 recv = Vector3Add((Vector3){ 0.32f, -0.42f, -1.28f }, off);
    DrawCubeV(recv, (Vector3){ 0.16f, 0.20f, 0.95f }, gunBody);
    DrawCubeWiresV(recv, (Vector3){ 0.16f, 0.20f, 0.95f }, gunDark);
    // receiver top glint
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.315f, -1.28f }, off), (Vector3){ 0.14f, 0.012f, 0.9f }, gunSteel);
    Vector3 hg = Vector3Add((Vector3){ 0.32f, -0.40f, -1.85f }, off);
    DrawCubeV(hg, (Vector3){ 0.14f, 0.16f, 0.42f }, woodCol);
    DrawCubeWiresV(hg, (Vector3){ 0.14f, 0.16f, 0.42f }, woodDark);
    // wood grain lines
    DrawCubeV(Vector3Add(hg, (Vector3){ 0, 0.02f, 0 }), (Vector3){ 0.145f, 0.015f, 0.40f }, woodDark);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.32f, -2.05f }, off), (Vector3){ 0.10f, 0.10f, 0.12f }, gunSteel);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.32f, -1.65f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.32f, -2.05f }, off), 0.025f, 0.025f, 8, gunSteel);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.42f, -2.05f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.42f, -2.55f }, off), 0.028f, 0.028f, 10, gunSteel);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.42f, -2.60f }, off), (Vector3){ 0.07f, 0.07f, 0.10f }, gunDark);
    Vector3 stock = Vector3Add((Vector3){ 0.34f, -0.46f, -0.55f }, off);
    DrawCubeV(stock, (Vector3){ 0.14f, 0.22f, 0.50f }, woodCol);
    DrawCubeWiresV(stock, (Vector3){ 0.14f, 0.22f, 0.50f }, woodDark);
    // charging handle pulls back
    Vector3 ch = Vector3Add((Vector3){ 0.43f, -0.36f, -1.05f }, off);
    ch.z += charge * 0.14f;
    DrawCubeV(ch, (Vector3){ 0.06f, 0.03f, 0.10f }, gunSteel);
    // left arm: bottom-left anchor -> working hand (follows the reload tilt)
    Vector3 lHandW = Vector3Add(lHand, off);
    DrawLimbBlocky(Vector3Add((Vector3){ -0.68f, -0.94f, -0.62f }, off), lHandW, 0.15f, skin);
    DrawCubeV(lHandW, (Vector3){ 0.21f, 0.21f, 0.21f }, skin);
    DrawCubeWiresV(lHandW, (Vector3){ 0.21f, 0.21f, 0.21f }, skin);
    // fresh mag rides in the left hand while it travels
    if (reloadT > 0.50f && reloadT < 0.68f) {
        DrawCubeV(Vector3Add(lHandW, (Vector3){ 0, -0.14f, 0 }), (Vector3){ 0.13f, 0.22f, 0.20f }, gunBody2);
        DrawCubeV(Vector3Add(lHandW, (Vector3){ 0, -0.30f, 0.03f }), (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
    }
    rlPopMatrix();

    // curved mag: old falls, new seats (opaque to avoid sort flicker)
    Vector3 magBase = Vector3Add((Vector3){ 0.32f, -0.60f, -1.15f }, off);
    if (reloadT > 0.0f && reloadT < 1.0f) {
        if (oldMag > 0.15f) {
            float d = 1.0f - oldMag;
            Vector3 mp = magBase;
            mp.y -= d * d * 0.7f;
            mp.z += d * 0.08f;
            DrawCubeV(mp, (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
        }
        if (newMag >= 0.99f) {
            DrawCubeV(magBase, (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
            DrawCubeV(Vector3Add(magBase, (Vector3){ 0, -0.15f, 0.02f }), (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
        }
        if (newMag < 0.05f && oldMag < 0.05f) {
            // magwell empty: dark hole
            DrawCubeV(magBase, (Vector3){ 0.11f, 0.10f, 0.18f }, BLACK);
        }
    } else {
        DrawCubeV(magBase, (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
        DrawCubeV(Vector3Add(magBase, (Vector3){ 0, -0.15f, 0.02f }), (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
        DrawCubeV(Vector3Add(magBase, (Vector3){ 0, -0.30f, 0.08f }), (Vector3){ 0.13f, 0.16f, 0.20f }, gunBody2);
    }

    if (muzzleTimer > 0.0f) {
        float t = muzzleTimer / MUZZLE_FLASH_TIME;
        float s = 0.075f * (0.7f + RandomFloat() * 0.6f) * (0.4f + 0.6f * t);
        Vector3 fp = Vector3Add(off, (Vector3){ 0.32f, -0.42f, -2.68f });
        DrawSphereEx(fp, s * 1.8f, 10, 8, (Color){ 255, 200, 80, 255 });
        DrawSphereEx(fp, s * 0.8f, 8, 6, (Color){ 255, 255, 240, 255 });
        DrawCubeV(fp, (Vector3){ s * 8.0f, s * 0.5f, s * 0.5f }, (Color){ 255, 230, 150, 255 });
        DrawCubeV(fp, (Vector3){ s * 0.5f, s * 8.0f, s * 0.5f }, (Color){ 255, 230, 150, 255 });
    }
}

static void DrawSniperViewmodel(Vector3 off, float muzzleTimer, float reloadT, float shotAge) {
    Color skin     = (Color){ 228, 178, 138, 255 };
    Color gunBody  = (Color){  38,  42,  34, 255 };
    Color gunBody2 = (Color){  28,  32,  26, 255 };
    Color gunDark  = (Color){  12,  14,  12, 255 };
    Color gunSteel = (Color){  70,  74,  68, 255 };
    Color steelLit = (Color){  96, 100,  94, 255 };
    Color scopeCol = (Color){  22,  24,  22, 255 };
    Color scopeLens= (Color){  60, 110, 160, 255 };
    Color woodCol  = (Color){  92,  62,  38, 255 };
    Color woodLit  = (Color){ 110,  76,  46, 255 };
    Color brass    = (Color){ 205, 168,  78, 255 };

    float roll = 0, ptilt = 0;
    ReloadTilt(reloadT, &off, &roll, &ptilt);
    SeatJolt(reloadT, 0.665f, &off, &roll, &ptilt);

    // ---- bolt cycle ------------------------------------------------------
    // Runs after every shot (shotAge = seconds since the trigger) AND during
    // the reload's charge phase. Phases: lift, pull back, hold, push, lock.
    float boltT = -1.0f;
    if (shotAge >= 0.0f && shotAge < 0.55f) boltT = shotAge / 0.55f;
    if (reloadT > 0.55f && reloadT < 0.90f) boltT = (reloadT - 0.55f) / 0.35f;
    float bLift = 0.0f, bBack = 0.0f, caseT = -1.0f;
    if (boltT >= 0.0f) {
        float up   = Phase(boltT, 0.02f, 0.16f);
        float down = Phase(boltT, 0.86f, 1.00f);
        bLift = up - down;
        float pull = Phase(boltT, 0.16f, 0.48f);
        float push = Phase(boltT, 0.62f, 0.88f);
        bBack = ClampF(pull - push, 0.0f, 1.0f);
        if (boltT > 0.18f && boltT < 0.62f) caseT = (boltT - 0.18f) / 0.44f;
    }
    // shot kick: receiver slams back, muzzle climbs, settles fast
    float kick = (shotAge >= 0.0f && shotAge < 0.4f) ? expf(-shotAge * 16.0f) : 0.0f;
    off.z += kick * 0.15f;
    off.y += kick * 0.020f;
    ptilt += kick * 3.2f;
    float triggerPull = kick;

    float oldMag = 1.0f - Phase(reloadT, 0.04f, 0.28f);
    float newMag = Phase(reloadT, 0.52f, 0.66f);
    // LEFT hand: cupped under the fore-end at rest; swaps the magazine
    static const HandKey keys[] = {
        { 0.00f, { 0.32f, -0.58f, -1.95f } },  // under the fore-end
        { 0.10f, { 0.33f, -0.78f, -1.30f } },  // slide back to the mag
        { 0.26f, { 0.31f, -0.98f, -1.24f } },  // rip the old mag out
        { 0.34f, { 0.22f, -1.14f, -1.02f } },  // clear it low
        { 0.44f, { -0.12f, -1.26f, -0.72f } }, // belt (off-screen)
        { 0.58f, { 0.33f, -0.84f, -1.26f } },  // fresh mag up to the well
        { 0.64f, { 0.32f, -0.70f, -1.26f } },  // rock + seat it
        { 1.00f, { 0.32f, -0.58f, -1.95f } },  // back to the fore-end
    };
    Vector3 lHand = HandPath(keys, 8, reloadT);
    lHand.y += sinf(s_animTime * 1.7f) * 0.004f;

    rlPushMatrix();
    rlTranslatef(0.32f, -0.40f, -1.35f);
    rlRotatef(roll, 0, 0, 1);
    rlRotatef(ptilt, 1, 0, 0);
    rlTranslatef(-0.32f, 0.40f, 1.35f);

    // flat-top receiver: rail, ejection port, bolt raceway, trigger group
    Vector3 recv = Vector3Add((Vector3){ 0.32f, -0.40f, -1.35f }, off);
    DrawCubeV(recv, (Vector3){ 0.18f, 0.22f, 1.30f }, gunBody);
    DrawCubeWiresV(recv, (Vector3){ 0.18f, 0.22f, 1.30f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.275f, -1.35f }, off), (Vector3){ 0.14f, 0.03f, 1.30f }, gunBody2);
    for (int i = 0; i < 5; i++)  // rail notches
        DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.258f, -0.85f - (float)i*0.22f }, off),
                  (Vector3){ 0.15f, 0.03f, 0.06f }, gunDark);
    // ejection port + bolt carrier sliding back with the cycle
    DrawCubeV(Vector3Add((Vector3){ 0.415f, -0.365f, -1.16f }, off), (Vector3){ 0.035f, 0.10f, 0.30f }, gunDark);
    Vector3 carrier = Vector3Add((Vector3){ 0.435f, -0.375f, -1.16f + bBack*0.36f }, off);
    DrawCubeV(carrier, (Vector3){ 0.05f, 0.06f, 0.24f }, steelLit);
    // trigger group (trigger rides back while the shot resolves)
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.545f, -0.98f }, off), (Vector3){ 0.07f, 0.025f, 0.20f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.515f, -0.875f }, off), (Vector3){ 0.07f, 0.06f, 0.025f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.505f + triggerPull*0.012f, -1.010f + triggerPull*0.03f }, off),
              (Vector3){ 0.035f, 0.07f, 0.025f }, gunSteel);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.525f, -1.30f }, off), (Vector3){ 0.11f, 0.025f, 0.42f }, gunBody2);

    // wooden butt-stock: cheek riser, grip, recoil pad, sling loop
    Vector3 stock = Vector3Add((Vector3){ 0.34f, -0.47f, -0.46f }, off);
    DrawCubeV(stock, (Vector3){ 0.19f, 0.30f, 0.66f }, woodCol);
    DrawCubeWiresV(stock, (Vector3){ 0.19f, 0.30f, 0.66f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.34f, -0.285f, -0.46f }, off), (Vector3){ 0.21f, 0.09f, 0.46f }, woodLit);
    DrawCubeV(Vector3Add((Vector3){ 0.34f, -0.47f, -0.115f }, off), (Vector3){ 0.21f, 0.32f, 0.055f }, gunDark);
    Vector3 grip = Vector3Add((Vector3){ 0.36f, -0.62f, -0.82f }, off);
    DrawCubeV(grip, (Vector3){ 0.13f, 0.30f, 0.17f }, woodCol);
    DrawCubeV(Vector3Add(grip, (Vector3){ 0, -0.05f, 0.02f }), (Vector3){ 0.14f, 0.08f, 0.19f }, woodLit);
    DrawCubeV(Vector3Add((Vector3){ 0.34f, -0.62f, -0.30f }, off), (Vector3){ 0.05f, 0.03f, 0.03f }, gunSteel);

    // heavy barrel: chamber, taper, ported muzzle brake, crown bore
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.40f, -1.72f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.40f, -2.06f }, off), 0.078f, 0.056f, 12, gunSteel);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.40f, -2.06f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.40f, -2.64f }, off), 0.050f, 0.044f, 12, gunSteel);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.40f, -2.64f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.40f, -2.84f }, off), 0.062f, 0.062f, 10, gunBody2);
    for (int i = 0; i < 3; i++)  // brake vents
        DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.40f, -2.68f - (float)i*0.055f }, off),
                  (Vector3){ 0.15f, 0.028f, 0.03f }, gunDark);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.40f, -2.845f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.40f, -2.86f }, off), 0.049f, 0.049f, 10, BLACK);

    // wooden fore-end + folded bipod
    Vector3 fore = Vector3Add((Vector3){ 0.32f, -0.525f, -1.99f }, off);
    DrawCubeV(fore, (Vector3){ 0.16f, 0.14f, 0.78f }, woodCol);
    DrawCubeWiresV(fore, (Vector3){ 0.16f, 0.14f, 0.78f }, gunDark);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.615f, -2.16f }, off), (Vector3){ 0.09f, 0.05f, 0.10f }, gunSteel);
    DrawCubeV(Vector3Add((Vector3){ 0.245f, -0.66f, -1.90f }, off), (Vector3){ 0.030f, 0.030f, 0.58f }, gunSteel);
    DrawCubeV(Vector3Add((Vector3){ 0.395f, -0.66f, -1.90f }, off), (Vector3){ 0.030f, 0.030f, 0.58f }, gunSteel);

    // scope: ring mounts, 30mm tube, objective bell + sunshade, turrets,
    // magnification ring with ridges, front/rear lens glints
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.225f, -1.12f }, off), (Vector3){ 0.09f, 0.15f, 0.10f }, gunSteel);
    DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.225f, -1.52f }, off), (Vector3){ 0.09f, 0.15f, 0.10f }, gunSteel);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -0.90f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -1.78f }, off), 0.070f, 0.070f, 12, scopeCol);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -1.78f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -1.94f }, off), 0.070f, 0.098f, 12, scopeCol);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -1.94f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -2.02f }, off), 0.098f, 0.098f, 12, gunBody2);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -2.025f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -2.035f }, off), 0.088f, 0.088f, 12, scopeLens);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -0.90f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -0.78f }, off), 0.070f, 0.086f, 12, scopeCol);
    // eyepiece glass reads dark like a real optic seen from behind
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -0.775f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -0.765f }, off), 0.042f, 0.042f, 10, (Color){ 26, 34, 46, 255 });
    // elevation turret (top) + windage turret (right)
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.09f, -1.34f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.005f, -1.34f }, off), 0.045f, 0.045f, 10, scopeCol);
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.005f, -1.34f }, off),
                   Vector3Add((Vector3){ 0.32f, 0.008f, -1.34f }, off), 0.050f, 0.050f, 10, gunDark);
    DrawCylinderEx(Vector3Add((Vector3){ 0.39f, -0.16f, -1.34f }, off),
                   Vector3Add((Vector3){ 0.475f, -0.16f, -1.34f }, off), 0.042f, 0.042f, 10, scopeCol);
    // magnification ring
    DrawCylinderEx(Vector3Add((Vector3){ 0.32f, -0.16f, -0.985f }, off),
                   Vector3Add((Vector3){ 0.32f, -0.16f, -1.035f }, off), 0.082f, 0.082f, 12, gunBody2);
    for (int i = 0; i < 6; i++) {
        float a = (float)i * PI / 3.0f;
        DrawCubeV(Vector3Add((Vector3){ 0.32f + cosf(a)*0.078f, -0.16f + sinf(a)*0.078f, -1.01f }, off),
                  (Vector3){ 0.02f, 0.02f, 0.045f }, gunDark);
    }
    // scope glint sweeps with time
    DrawCubeV(Vector3Add((Vector3){ 0.30f, -0.14f, -1.20f }, off), (Vector3){ 0.02f, 0.02f, 0.30f }, (Color){ 180, 220, 255, 60 });

    // bolt handle + knob: lifts, pulls back, slams home with the cycle
    Vector3 bp = Vector3Add((Vector3){ 0.455f, -0.365f, -1.16f }, off);
    bp.z += bBack * 0.36f;
    bp.y += bLift * 0.055f;
    DrawCubeV(bp, (Vector3){ 0.17f, 0.045f, 0.045f }, steelLit);
    DrawSphere(Vector3Add(bp, (Vector3){ 0.09f, 0, 0 }), 0.038f, steelLit);

    // RIGHT arm: rides the grip; slides up to run the bolt while cycling
    Vector3 hp = Vector3Add((Vector3){ 0.38f, -0.56f, -0.92f }, off);
    if (bLift > 0.001f) {
        Vector3 boltHand = Vector3Add(bp, (Vector3){ 0.02f, 0.01f, 0.0f });
        float g = ClampF(bLift * 1.35f, 0.0f, 1.0f);
        hp.x += (boltHand.x - hp.x) * g;
        hp.y += (boltHand.y - hp.y) * g;
        hp.z += (boltHand.z - hp.z) * g;
    }
    DrawLimbBlocky(Vector3Add((Vector3){ 0.76f, -0.96f, -0.58f }, off), hp, 0.195f, skin);
    DrawCubeV(hp, (Vector3){ 0.24f, 0.24f, 0.24f }, skin);

    // ejected brass case arcing out of the port while the bolt is back
    if (caseT >= 0.0f) {
        Vector3 cp = Vector3Add((Vector3){ 0.46f + caseT*0.20f,
                                           -0.33f + caseT*0.26f - caseT*caseT*0.34f,
                                           -1.10f + caseT*0.16f }, off);
        rlPushMatrix();
        rlTranslatef(cp.x, cp.y, cp.z);
        rlRotatef(caseT * 720.0f, 0.5f, 1.0f, 0.3f);
        DrawCubeV((Vector3){ 0, 0, 0 }, (Vector3){ 0.045f, 0.045f, 0.085f }, brass);
        rlPopMatrix();
    }

    // LEFT arm: bottom-left anchor -> working hand (follows the reload tilt)
    Vector3 lHandW = Vector3Add(lHand, off);
    DrawLimbBlocky(Vector3Add((Vector3){ -0.68f, -0.96f, -0.60f }, off), lHandW, 0.15f, skin);
    DrawCubeV(lHandW, (Vector3){ 0.21f, 0.21f, 0.21f }, skin);
    DrawCubeWiresV(lHandW, (Vector3){ 0.21f, 0.21f, 0.21f }, skin);
    // fresh mag rides in the left hand while it travels
    if (reloadT > 0.42f && reloadT < 0.68f)
        DrawCubeV(Vector3Add(lHandW, (Vector3){ 0, -0.15f, 0.02f }), (Vector3){ 0.12f, 0.22f, 0.24f }, gunBody2);
    rlPopMatrix();

    // detachable box magazine (drops free, gets replaced, floorplate lip)
    if (reloadT > 0.0f && reloadT < 1.0f) {
        if (oldMag > 0.15f) {
            float drop = (1.0f - oldMag);
            Vector3 mp = Vector3Add((Vector3){ 0.32f, -0.64f, -1.26f }, off);
            mp.y -= drop * drop * 0.7f;
            DrawCubeV(mp, (Vector3){ 0.12f, 0.24f, 0.24f }, gunBody2);
        }
        if (newMag >= 0.99f)
            DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.64f, -1.26f }, off), (Vector3){ 0.12f, 0.24f, 0.24f }, gunBody2);
        if (oldMag < 0.05f && newMag < 0.05f)
            DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.64f, -1.26f }, off), (Vector3){ 0.10f, 0.13f, 0.22f }, BLACK);
    } else {
        DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.64f, -1.26f }, off), (Vector3){ 0.12f, 0.24f, 0.24f }, gunBody2);
        DrawCubeV(Vector3Add((Vector3){ 0.32f, -0.755f, -1.26f }, off), (Vector3){ 0.13f, 0.025f, 0.25f }, gunDark);
    }

    if (muzzleTimer > 0.0f) {
        float t = muzzleTimer / MUZZLE_FLASH_TIME;
        float s = 0.13f * (0.7f + RandomFloat() * 0.6f) * (0.4f + 0.6f * t);
        Vector3 fp = Vector3Add(off, (Vector3){ 0.32f, -0.40f, -2.90f });
        DrawSphereEx(fp, s * 1.9f, 10, 8, (Color){ 255, 200, 80, 255 });
        DrawSphereEx(fp, s * 0.9f, 8, 6, (Color){ 255, 255, 240, 255 });
        DrawCubeV(fp, (Vector3){ s * 10.0f, s * 0.6f, s * 0.6f }, (Color){ 255, 230, 150, 255 });
        DrawCubeV(fp, (Vector3){ s * 0.6f, s * 10.0f, s * 0.6f }, (Color){ 255, 230, 150, 255 });
        DrawCubeV(fp, (Vector3){ s * 0.6f, s * 0.6f, s * 8.0f }, (Color){ 255, 220, 140, 200 });
    }
}

void Weapon_DrawViewmodel(int index, Vector3 off, float muzzleTimer, float reloadT,
                          float adsBlend, float time, float sprintBlend, float moveRoll,
                          float shotAge) {
    s_animTime = time;
    float ac = 1.0f - adsBlend * 0.7f;
    float sc = 1.0f - sprintBlend;
    off.y += sinf(time * 1.7f) * 0.0035f * ac * sc;
    off.x += cosf(time * 1.3f) * 0.0030f * ac * sc;
    // sprint: runner sway + a hint of random stumble
    off.x += sinf(time * 6.2f) * 0.011f * sprintBlend;
    off.y += fabsf(cosf(time * 6.2f)) * 0.009f * sprintBlend;
    off = ApplyAds(off, adsBlend);
    // weapon drops to a low-ready carry while sprinting; strafing rolls it
    off.y -= 0.16f * sprintBlend;
    off.x += 0.05f * sprintBlend;
    off.z += 0.09f * sprintBlend;
    rlPushMatrix();
    rlTranslatef(0.32f, -0.52f, -1.0f);
    rlRotatef(-36.0f * sprintBlend, 1, 0, 0);
    rlRotatef(12.0f * sprintBlend + moveRoll, 0, 0, 1);
    rlTranslatef(-0.32f, 0.52f, 1.0f);
    if (index == 0) DrawPistolViewmodel(off, muzzleTimer, reloadT);
    else if (index == 1) DrawAKRViewmodel(off, muzzleTimer, reloadT);
    else DrawSniperViewmodel(off, muzzleTimer, reloadT, shotAge);
    rlPopMatrix();
}

void Weapon_DrawScopeOverlay(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float scopeSize = (float)sh * 0.86f;
    float left   = (sw - scopeSize) * 0.5f;
    float top    = (sh - scopeSize) * 0.5f;
    float right  = left + scopeSize;
    float bottom = top + scopeSize;
    DrawRectangle(0, 0, sw, (int)top, BLACK);
    DrawRectangle(0, (int)bottom, sw, sh - (int)bottom, BLACK);
    DrawRectangle(0, (int)top, (int)left, (int)scopeSize, BLACK);
    DrawRectangle((int)right, (int)top, sw - (int)right, (int)scopeSize, BLACK);
    Vector2 center = { sw * 0.5f, sh * 0.5f };
    float r = scopeSize * 0.5f;
    DrawCircleLinesV(center, r, BLACK);
    DrawCircleLinesV(center, r - 2.0f, (Color){ 60, 60, 60, 255 });
    DrawCircleLinesV(center, r - 4.0f, (Color){ 25, 25, 25, 255 });
    DrawCircleV(center, r - 6, (Color){ 120, 160, 200, 14 });
    float glint = sinf((float)GetTime() * 0.8f) * r * 0.15f;
    DrawCircleV((Vector2){ center.x - r * 0.25f + glint * 0.2f, center.y - r * 0.28f }, r * 0.10f, (Color){ 255, 255, 255, 16 });
    // range ticks glow
    Color lineCol = (Color){ 15, 15, 15, 255 };
    DrawLine((int)(center.x - r + 6), (int)center.y, (int)(center.x - 8), (int)center.y, lineCol);
    DrawLine((int)(center.x + 8), (int)center.y, (int)(center.x + r - 6), (int)center.y, lineCol);
    DrawLine((int)center.x, (int)(center.y - r + 6), (int)center.x, (int)(center.y - 8), lineCol);
    DrawLine((int)center.x, (int)(center.y + 8), (int)center.x, (int)(center.y + r - 6), lineCol);
    for (int i = 1; i <= 5; i++) {
        int o = i * 22;
        DrawLine((int)(center.x - 4), (int)(center.y + o), (int)(center.x + 4), (int)(center.y + o), lineCol);
        DrawLine((int)(center.x - 4), (int)(center.y - o), (int)(center.x + 4), (int)(center.y - o), lineCol);
        DrawLine((int)(center.x + o), (int)(center.y - 4), (int)(center.x + o), (int)(center.y + 4), lineCol);
        DrawLine((int)(center.x - o), (int)(center.y - 4), (int)(center.x - o), (int)(center.y + 4), lineCol);
    }
    DrawCircleV(center, 1.8f, (Color){ 220, 40, 40, 255 });
    DrawCircleV(center, 4.5f, (Color){ 220, 40, 40, 60 });
}

void Weapon_DrawHitMarker(float timer, int type) {
    if (timer <= 0.0f) return;
    float t = timer / HITMARKER_TIME;
    unsigned char a = (unsigned char)(255 * t);
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    Color col; float size, gap, thick;
    if (type == 4)      { col = (Color){ 255, 60, 60, a };   size = 22 + (1-t)*10; gap = 7; thick = 3.5f; } // headshot kill
    else if (type == 3) { col = (Color){ 255, 70, 70, a };   size = 18 + (1-t)*8; gap = 6; thick = 3.0f; }
    else if (type == 2) { col = (Color){ 255, 215, 70, a };  size = 15 + (1-t)*5; gap = 5; thick = 2.5f; }
    else                { col = (Color){ 245, 245, 245, a }; size = 11; gap = 4; thick = 2.0f; }
    Vector2 c = { (float)cx, (float)cy };
    DrawLineEx((Vector2){ c.x - size, c.y - size }, (Vector2){ c.x - gap, c.y - gap }, thick, col);
    DrawLineEx((Vector2){ c.x + size, c.y - size }, (Vector2){ c.x + gap, c.y - gap }, thick, col);
    DrawLineEx((Vector2){ c.x - size, c.y + size }, (Vector2){ c.x - gap, c.y + gap }, thick, col);
    DrawLineEx((Vector2){ c.x + size, c.y + size }, (Vector2){ c.x + gap, c.y + gap }, thick, col);
    if (type == 4 || type == 2) {
        // skull dot for headshots
        DrawCircleV(c, 3.0f, col);
    }
}
