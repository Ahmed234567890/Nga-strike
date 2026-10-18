#include "fx.h"
#include "rlgl.h"
#include <math.h>

void FX_Init(Particle* p, int pc, Tracer* t, int tc, Decal* d, int dc) {
    for (int i = 0; i < pc; i++) p[i].life = 0;
    for (int i = 0; i < tc; i++) t[i].timer = 0;
    for (int i = 0; i < dc; i++) d[i].timer = 0;
}

static void SpawnOne(Particle* p, int pc, Vector3 pos, Vector3 vel,
                     float life, float s0, float s1, Color c, float grav, float drag) {
    int best = -1;
    for (int i = 0; i < pc; i++) {
        if (p[i].life <= 0.0f) { best = i; break; }
    }
    if (best < 0) {
        best = 0;
        for (int i = 1; i < pc; i++) if (p[i].life < p[best].life) best = i;
    }
    p[best].position = pos;
    p[best].velocity = vel;
    p[best].life = life;
    p[best].maxLife = life;
    p[best].size0 = s0;
    p[best].size1 = s1;
    p[best].color = c;
    p[best].gravity = grav;
    p[best].drag = drag;
}

void FX_SpawnSpark(Particle* p, int pc, Vector3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Vector3 v = { RandRange(-5, 5), RandRange(0.5f, 6.0f), RandRange(-5, 5) };
        Color c = (i & 1) ? (Color){ 255, 210, 110, 255 } : (Color){ 255, 120, 40, 255 };
        if ((i % 5) == 0) c = (Color){ 255, 255, 240, 255 };
        SpawnOne(p, pc, pos, v, RandRange(0.15f, 0.5f), RandRange(0.03f, 0.075f), 0.01f, c, -11.0f, 1.2f);
    }
}

void FX_SpawnBlood(Particle* p, int pc, Vector3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Vector3 v = { RandRange(-3.2f, 3.2f), RandRange(-0.5f, 3.8f), RandRange(-3.2f, 3.2f) };
        Color c = (i & 3) == 0 ? (Color){ 120, 8, 8, 255 } : (Color){ 185, 22, 22, 255 };
        SpawnOne(p, pc, pos, v, RandRange(0.25f, 0.6f), RandRange(0.045f, 0.095f), 0.015f, c, -13.0f, 0.8f);
    }
}

void FX_SpawnHeadBlood(Particle* p, int pc, Vector3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Vector3 v = { RandRange(-4.5f, 4.5f), RandRange(0.0f, 5.0f), RandRange(-4.5f, 4.5f) };
        SpawnOne(p, pc, pos, v, RandRange(0.3f, 0.7f), RandRange(0.05f, 0.11f), 0.015f,
                 (Color){ 210, 30, 30, 255 }, -13.0f, 0.7f);
    }
    // fine mist
    for (int i = 0; i < n / 2; i++) {
        Vector3 v = { RandRange(-1.2f, 1.2f), RandRange(0.4f, 1.6f), RandRange(-1.2f, 1.2f) };
        SpawnOne(p, pc, pos, v, RandRange(0.5f, 1.0f), 0.10f, 0.30f,
                 (Color){ 160, 20, 20, 120 }, 0.8f, 1.6f);
    }
}

void FX_SpawnSmoke(Particle* p, int pc, Vector3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Vector3 v = { RandRange(-0.4f, 0.4f), RandRange(0.6f, 1.5f), RandRange(-0.4f, 0.4f) };
        SpawnOne(p, pc, pos, v, RandRange(0.4f, 0.9f), 0.08f, 0.24f,
                 (Color){ 200, 200, 200, 130 }, 1.2f, 1.2f);
    }
}

void FX_SpawnMuzzleSmoke(Particle* p, int pc, Vector3 pos, Vector3 dir, int n) {
    // Small and forward: the muzzle sits 0.6m from the camera, so big/long
    // smoke turns into screen-filling blobs when firing.
    for (int i = 0; i < n; i++) {
        Vector3 at = Vector3Add(pos, Vector3Scale(dir, 0.45f));
        Vector3 v = Vector3Add(Vector3Scale(dir, RandRange(1.5f, 2.5f)),
                                (Vector3){ RandRange(-0.3f, 0.3f), RandRange(0.2f, 0.7f), RandRange(-0.3f, 0.3f) });
        SpawnOne(p, pc, at, v, RandRange(0.25f, 0.45f), 0.04f, 0.12f,
                 (Color){ 210, 205, 195, 90 }, 1.5f, 2.0f);
    }
}

void FX_SpawnPickupBurst(Particle* p, int pc, Vector3 pos, Color color) {
    for (int i = 0; i < 14; i++) {
        float a = ((float)i / 14.0f) * 2.0f * PI;
        Vector3 v = { cosf(a) * 2.5f, 2.0f, sinf(a) * 2.5f };
        SpawnOne(p, pc, pos, v, RandRange(0.3f, 0.6f), 0.06f, 0.01f, color, -4.0f, 1.5f);
    }
}

void FX_SpawnCasing(Particle* p, int pc, Vector3 pos, Vector3 right) {
    Vector3 v = Vector3Add(Vector3Scale(right, RandRange(1.2f, 2.2f)),
                            (Vector3){ RandRange(-0.3f, 0.3f), RandRange(1.5f, 2.6f), RandRange(-0.3f, 0.3f) });
    SpawnOne(p, pc, pos, v, RandRange(0.5f, 0.9f), 0.035f, 0.03f,
             (Color){ 232, 190, 90, 255 }, -9.5f, 0.4f);
}

void FX_SpawnHelmetPop(Particle* p, int pc, Vector3 pos) {
    for (int i = 0; i < 8; i++) {
        Vector3 v = { RandRange(-3, 3), RandRange(2.0f, 5.0f), RandRange(-3, 3) };
        Color c = (i & 1) ? (Color){ 60, 62, 66, 255 } : (Color){ 95, 98, 105, 255 };
        SpawnOne(p, pc, pos, v, RandRange(0.4f, 0.8f), 0.07f, 0.05f, c, -11.0f, 0.6f);
    }
    FX_SpawnSpark(p, pc, pos, 4);
}

void FX_SpawnExplosion(Particle* p, int pc, Vector3 pos, int power) {
    int n = 18 + power * 5;
    // triple-layer flash core: white -> orange flame -> deep red afterglow
    SpawnOne(p, pc, pos, (Vector3){ 0, 0.5f, 0 }, 0.22f, 1.6f + power * 0.30f, 0.3f,
             (Color){ 255, 240, 200, 255 }, 0, 3.0f);
    SpawnOne(p, pc, pos, (Vector3){ 0, 1.0f, 0 }, 0.35f, 1.1f + power * 0.25f, 0.4f,
             (Color){ 255, 160, 60, 230 }, 0, 2.5f);
    SpawnOne(p, pc, pos, (Vector3){ 0, 1.7f, 0 }, 0.52f, 1.5f + power * 0.28f, 0.5f,
             (Color){ 220, 80, 25, 190 }, 0.5f, 1.8f);
    // fire ball fragments
    for (int i = 0; i < n; i++) {
        float a = RandomFloat() * 2 * PI;
        float up = RandRange(1.0f, 8.5f);
        float sp = RandRange(2.0f, 10.5f);
        Vector3 v = { cosf(a) * sp, up, sinf(a) * sp };
        Color c = (i & 1) ? (Color){ 255, 180, 80, 255 } : (Color){ 80, 75, 70, 255 };
        SpawnOne(p, pc, pos, v, RandRange(0.45f, 1.3f), RandRange(0.06f, 0.16f), 0.02f, c, -8.0f, 1.0f);
    }
    // heavy debris chunks that arc out and bounce through the scene
    for (int i = 0; i < 6 + power; i++) {
        float a = RandomFloat() * 2 * PI;
        float sp = RandRange(3.0f, 8.0f);
        Vector3 v = { cosf(a) * sp, RandRange(3.0f, 9.5f), sinf(a) * sp };
        Color c = (i & 1) ? (Color){ 120, 42, 30, 255 } : (Color){ 90, 88, 84, 255 };
        SpawnOne(p, pc, pos, v, RandRange(1.2f, 2.1f), RandRange(0.05f, 0.11f), 0.03f, c, -14.0f, 0.8f);
    }
    // smoke column: fast burst + slow lingering cap that rises
    for (int i = 0; i < 12; i++) {
        Vector3 v = { RandRange(-2.0f, 2.0f), RandRange(1.2f, 4.0f), RandRange(-2.0f, 2.0f) };
        SpawnOne(p, pc, pos, v, RandRange(0.9f, 1.8f), 0.28f, 1.1f,
                 (Color){ 60, 58, 55, 160 }, 1.0f, 1.4f);
    }
    for (int i = 0; i < 4; i++) {
        Vector3 v = { RandRange(-0.6f, 0.6f), RandRange(1.6f, 2.6f), RandRange(-0.6f, 0.6f) };
        SpawnOne(p, pc, pos, v, RandRange(1.8f, 3.0f), 0.5f, 1.4f,
                 (Color){ 40, 40, 40, 70 }, -0.4f, 0.9f);
    }
}

void FX_SpawnDustMote(Particle* p, int pc, Vector3 center, float range) {
    Vector3 pos = { center.x + RandRange(-range, range), RandRange(0.3f, 3.5f), center.z + RandRange(-range, range) };
    Vector3 v = { RandRange(-0.25f, 0.25f), RandRange(-0.05f, 0.12f), RandRange(-0.25f, 0.25f) };
    SpawnOne(p, pc, pos, v, RandRange(2.5f, 5.0f), 0.02f, 0.008f,
             (Color){ 255, 250, 235, 90 }, 0.05f, 0.1f);
}

void FX_SpawnDirtKick(Particle* p, int pc, Vector3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Vector3 v = { RandRange(-1.5f, 1.5f), RandRange(0.8f, 2.5f), RandRange(-1.5f, 1.5f) };
        SpawnOne(p, pc, pos, v, RandRange(0.3f, 0.7f), RandRange(0.04f, 0.08f), 0.02f,
                 (Color){ 150, 130, 100, 200 }, -8.0f, 1.0f);
    }
}

void FX_AddTracer(Tracer* t, int tc, Vector3 a, Vector3 b, Color color) {
    // Guard degenerate beams: zero-length cylinders corrupt the render batch
    // and make later objects vanish for a frame. Point-blank needs no beam.
    if (!isfinite(a.x + a.y + a.z + b.x + b.y + b.z)) return;
    if (Vector3Distance(a, b) < 0.12f) return;
    int best = 0;
    for (int i = 0; i < tc; i++) {
        if (t[i].timer <= 0.0f) { best = i; break; }
        if (t[i].timer < t[best].timer) best = i;
    }
    t[best].start = a;
    t[best].end = b;
    t[best].timer = t[best].maxTime = 0.10f;
    t[best].color = color;
}

void FX_AddDecal(Decal* d, int dc, Vector3 pos, float size, Color color, int* cursor) {
    int i = (*cursor) % dc;
    (*cursor)++;
    d[i].position = pos;
    d[i].timer = d[i].maxTime = 25.0f;
    d[i].size = size;
    d[i].color = color;
}

void FX_Update(Particle* p, int pc, Tracer* t, int tc, Decal* d, int dc, float dt) {
    for (int i = 0; i < pc; i++) {
        if (p[i].life <= 0) continue;
        p[i].life -= dt;
        p[i].velocity.y += p[i].gravity * dt;
        float dr = 1.0f - ClampF(p[i].drag * dt, 0.0f, 0.9f);
        p[i].velocity = Vector3Scale(p[i].velocity, dr);
        p[i].position = Vector3Add(p[i].position, Vector3Scale(p[i].velocity, dt));
        if (p[i].position.y < 0.02f) { p[i].position.y = 0.02f; p[i].velocity.y *= -0.35f; p[i].velocity.x *= 0.7f; p[i].velocity.z *= 0.7f; }
    }
    for (int i = 0; i < tc; i++) if (t[i].timer > 0) t[i].timer -= dt;
    for (int i = 0; i < dc; i++) if (d[i].timer > 0) d[i].timer -= dt;
}

void FX_Draw(const Particle* p, int pc, const Tracer* t, int tc, const Decal* d, int dc) {
    // Transparent FX must not write depth: otherwise smoke/shadows erase
    // objects behind them and things pop in/out. Test stays on, write off.
    // CRITICAL: flush the render batch before touching GL state, otherwise
    // already-batched opaque geometry gets drawn with the new state and
    // flickers/disappears whenever the camera moves.
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    // Opaque-ish decals first as flattened slabs (no z-fight wires)
    for (int i = 0; i < dc; i++) {
        if (d[i].timer <= 0) continue;
        float a = ClampF(d[i].timer / d[i].maxTime, 0.0f, 1.0f);
        Color c = d[i].color;
        if (a < 0.25f) c.a = (unsigned char)(c.a * (a / 0.25f));
        Vector3 pos = d[i].position;
        pos.y += 0.08f; // lift above road (0.06) / shadow (0.14-0.15) layers
        if (c.a < 8) continue;
        DrawSphereEx(pos, d[i].size, 8, 5, c);
    }
    for (int i = 0; i < tc; i++) {
        if (t[i].timer <= 0) continue;
        float a = t[i].timer / t[i].maxTime;
        if (a <= 0) continue;
        Color c = t[i].color;
        c.a = (unsigned char)(255 * a);
        float len = Vector3Distance(t[i].start, t[i].end);
        if (len < 0.12f || !isfinite(len)) continue;
        // thick beam (visible at high fps) + core line
        DrawCylinderEx(t[i].start, t[i].end, 0.02f, 0.02f, 6, c);
        DrawSphereEx(t[i].end, 0.045f + 0.04f * a, 8, 6, c);
    }
    for (int i = 0; i < pc; i++) {
        if (p[i].life <= 0) continue;
        float t01 = ClampF(p[i].life / p[i].maxLife, 0.0f, 1.0f);
        float s = p[i].size1 + (p[i].size0 - p[i].size1) * t01;
        if (s < 0.008f || !isfinite(s)) continue;
        Color c = p[i].color;
        c.a = (unsigned char)(c.a * ClampF(t01 * 1.5f, 0.0f, 1.0f));
        if (c.a < 6) continue;
        // low-seg spheres: 6x4 for small, 10x8 for large flashes (perf + less shimmer)
        if (s > 0.3f) DrawSphereEx(p[i].position, s, 10, 8, c);
        else DrawSphereEx(p[i].position, s, 6, 4, c);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void FX_DrawShockwaves(const Tracer* t, int tc) {
    (void)t; (void)tc;
}
