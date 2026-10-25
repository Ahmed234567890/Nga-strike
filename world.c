#include "world.h"
#include <string.h>

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

static void World_Classic(WorldMap* map) {
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
    for (int i = 0; i < n; i++) map->boxes[i] = b[i];
    map->boxCount = n;

    // Metadata used by spawning, loot, lighting and net sync. These are the
    // original v3 positions, kept exactly so CLASSIC behaves as before.
    Vector3 players[WORLD_MAX_PLAYER_SPAWNS] = {
        {  0.0f, 0.0f,  12.0f }, { -11.0f, 0.0f, -6.0f },
        { 10.0f, 0.0f,  22.0f }, {   0.0f, 0.0f, -20.0f },
    };
    for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++) map->playerSpawns[i] = players[i];

    Vector3 enemies[MAX_ENEMIES] = {
        {  20.0f, 0.9f, -20.0f }, { -20.0f, 0.9f,  20.0f },
        {   0.0f, 0.9f, -35.0f }, {  30.0f, 0.9f,  10.0f },
        { -30.0f, 0.9f, -10.0f }, {   0.0f, 0.9f,  38.0f },
    };
    for (int i = 0; i < MAX_ENEMIES; i++) map->enemySpawns[i] = enemies[i];
    map->enemySpawnCount = MAX_ENEMIES;

    Vector3 barrels[MAX_BARRELS] = {
        { -8, 0, 6 }, { 8, 0, -6 }, { -14, 0, -14 },
        { 14, 0, 14 }, { 0, 0, -12 }, { -6, 0, 30 },
    };
    for (int i = 0; i < MAX_BARRELS; i++) map->barrelSpots[i] = barrels[i];
    map->barrelCount = MAX_BARRELS;

    Vector3 pickups[MAX_PICKUPS] = {
        { -30, 1.0f,  -5 }, {  30, 1.0f,   5 }, {   6, 1.0f,   9 },
        { -25, 1.0f, -25 }, {  25, 1.0f,  25 }, { -30, 1.0f,  25 },
        { 36.5f, 4.5f, 25 }, {   0, 1.0f,  35 },
    };
    PickupType kinds[MAX_PICKUPS] = {
        PICKUP_AMMO, PICKUP_AMMO, PICKUP_AMMO, PICKUP_AMMO,
        PICKUP_HEALTH, PICKUP_HEALTH, PICKUP_HEALTH, PICKUP_AMMO,
    };
    for (int i = 0; i < 8; i++) {
        map->pickupSpots[i] = pickups[i];
        map->pickupTypes[i] = kinds[i];
    }
    map->pickupCount = 8;

    Vector3 street[WORLD_MAX_STREET_LAMPS] = {
        { -12, 4.6f, -12 }, { 12, 4.6f, 12 }, { -12, 4.6f, 14 }, { 14, 4.6f, -12 },
    };
    for (int i = 0; i < WORLD_MAX_STREET_LAMPS; i++) map->streetLamps[i] = street[i];
    map->streetLampCount = WORLD_MAX_STREET_LAMPS;

    Vector3 accents[WORLD_MAX_ACCENT_LAMPS] = {
        { -14, 3.1f, -48.7f }, { -30, 3.2f, 21.05f },
    };
    for (int i = 0; i < WORLD_MAX_ACCENT_LAMPS; i++) map->accentLamps[i] = accents[i];
    map->accentLampCount = WORLD_MAX_ACCENT_LAMPS;

    map->mode = WORLD_CLASSIC;
    map->seed = 0;
    map->limit = 48.0f;
    map->groundSize = GROUND_SIZE;
}

// ---------------------------------------------------------------------------
// Deterministic random arenas
// ---------------------------------------------------------------------------

typedef struct {
    float x, z, r;
} ReservedSpot;

static uint32_t s_rng;

static uint32_t RngU32(void) {
    // mulberry32: small, deterministic and good enough for 2D layout.
    uint32_t z = (s_rng += 0x9E3779B9u);
    z ^= z >> 16;
    z *= 0x7FEB352Du;
    z ^= z >> 15;
    z *= 0x846CA68Bu;
    z ^= z >> 16;
    return z;
}

static float RngF(float a, float b) {
    return a + (b - a) * (float)(RngU32() >> 8) * (1.0f / 16777216.0f);
}

static int RngI(int a, int b) {
    if (b <= a) return a;
    return a + (int)(RngU32() % (uint32_t)(b - a + 1));
}

static bool BoxesOverlapExpanded(Box a, Box b, float clearance) {
    return fabsf(a.center.x - b.center.x) < a.halfExtents.x + b.halfExtents.x + clearance &&
           fabsf(a.center.y - b.center.y) < a.halfExtents.y + b.halfExtents.y + clearance &&
           fabsf(a.center.z - b.center.z) < a.halfExtents.z + b.halfExtents.z + clearance;
}

static bool BoxClearOfReserved(Box b, const ReservedSpot* zones, int zoneCount, float clearance) {
    for (int i = 0; i < zoneCount; i++) {
        float dx = fabsf(b.center.x - zones[i].x) - b.halfExtents.x;
        float dz = fabsf(b.center.z - zones[i].z) - b.halfExtents.z;
        if (dx < 0) dx = 0;
        if (dz < 0) dz = 0;
        float rr = zones[i].r + clearance;
        if (dx * dx + dz * dz < rr * rr) return false;
    }
    return true;
}

static bool BoxInsideBounds(Box b, float limit) {
    return fabsf(b.center.x) + b.halfExtents.x < limit - 1.5f &&
           fabsf(b.center.z) + b.halfExtents.z < limit - 1.5f;
}

static void AddBox(WorldMap* map, Box b) {
    if (map->boxCount >= WORLD_MAX_BOXES) return;
    map->boxes[map->boxCount++] = b;
}

static void Reserve(ReservedSpot* zones, int* count, Vector3 p, float radius) {
    if (*count >= 128) return;
    zones[*count] = (ReservedSpot){ p.x, p.z, radius };
    (*count)++;
}

static bool PointClearOfReserved(float x, float z, const ReservedSpot* zones, int count, float radius) {
    for (int i = 0; i < count; i++) {
        float dx = x - zones[i].x, dz = z - zones[i].z;
        float rr = zones[i].r + radius;
        if (dx * dx + dz * dz < rr * rr) return false;
    }
    return true;
}

static bool ConnectivityOk(const WorldMap* map) {
    enum { GRID_MAX = 130 };
    static unsigned char blocked[GRID_MAX * GRID_MAX];
    static unsigned char seen[GRID_MAX * GRID_MAX];
    static int queue[GRID_MAX * GRID_MAX];
    const float cell = 2.0f;
    const float start = -map->limit + 2.0f;
    int n = (int)((2.0f * (map->limit - 2.0f)) / cell);
    if (n > GRID_MAX) n = GRID_MAX;
    if (n < 8) return false;
    memset(blocked, 0, (size_t)n * n);
    memset(seen, 0, (size_t)n * n);

    for (int z = 0; z < n; z++) {
        for (int x = 0; x < n; x++) {
            float wx = start + ((float)x + 0.5f) * cell;
            float wz = start + ((float)z + 0.5f) * cell;
            Vector3 p = { wx, 0.9f, wz };
            Vector3 half = { 0.42f, 0.85f, 0.42f };
            blocked[z * n + x] = World_MoveCollides(p, half, map->boxes, map->boxCount) ? 1 : 0;
        }
    }

    int sx = (int)(((map->playerSpawns[0].x) - start) / cell);
    int sz = (int)(((map->playerSpawns[0].z) - start) / cell);
    if (sx < 0 || sz < 0 || sx >= n || sz >= n) return false;
    int head = 0, tail = 0;
    if (blocked[sz * n + sx]) return false;
    seen[sz * n + sx] = 1;
    queue[tail++] = sz * n + sx;
    const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    while (head < tail) {
        int id = queue[head++];
        int cx = id % n, cz = id / n;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dirs[d][0], nz = cz + dirs[d][1];
            if (nx < 0 || nz < 0 || nx >= n || nz >= n) continue;
            int ni = nz * n + nx;
            if (seen[ni] || blocked[ni]) continue;
            seen[ni] = 1;
            queue[tail++] = ni;
        }
    }

    // Every spawn, barrel, lamp and loot point must be reachable from player 0.
    Vector3 checks[4 + MAX_ENEMIES + MAX_BARRELS + MAX_PICKUPS + WORLD_MAX_STREET_LAMPS];
    int cc = 0;
    for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++) checks[cc++] = map->playerSpawns[i];
    for (int i = 0; i < map->enemySpawnCount; i++) checks[cc++] = map->enemySpawns[i];
    for (int i = 0; i < map->barrelCount; i++) checks[cc++] = map->barrelSpots[i];
    for (int i = 0; i < map->pickupCount; i++) checks[cc++] = map->pickupSpots[i];
    for (int i = 0; i < map->streetLampCount; i++) checks[cc++] = map->streetLamps[i];
    for (int i = 0; i < cc; i++) {
        int cx = (int)((checks[i].x - start) / cell);
        int cz = (int)((checks[i].z - start) / cell);
        bool found = false;
        for (int dz = -2; dz <= 2 && !found; dz++) {
            for (int dx = -2; dx <= 2 && !found; dx++) {
                int x = cx + dx, z = cz + dz;
                if (x < 0 || z < 0 || x >= n || z >= n) continue;
                if (seen[z * n + x]) found = true;
            }
        }
        if (!found) return false;
    }
    return true;
}

static bool CoverCandidateOk(const WorldMap* map, const Box* cand, int n,
                              const ReservedSpot* zones, int zoneCount) {
    for (int i = 0; i < n; i++) {
        if (!BoxInsideBounds(cand[i], map->limit)) return false;
        if (!BoxClearOfReserved(cand[i], zones, zoneCount, 0.55f)) return false;
        for (int j = 0; j < map->boxCount; j++) {
            float gap = 0.65f;
            if (BoxesOverlapExpanded(cand[i], map->boxes[j], gap)) return false;
        }
        for (int j = i + 1; j < n; j++) {
            if (BoxesOverlapExpanded(cand[i], cand[j], 0.0f)) return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Random city generator
//
// The layout is a road grid with a designed site in every block. Sites are
// built inside their block footprint, gates face the surrounding streets and
// the centre of every road stays clear. That means connectivity is guaranteed
// by construction instead of being rescued by scattering filler cover.
// ---------------------------------------------------------------------------

typedef struct {
    bool large;
    int cols;
    float coords[4];
    float blockHalf;
    float siteHalf;
    float roadHalf;
    float edgeRoad;
} CityLayout;

typedef struct {
    Box boxes[30];
    int n;
    Vector3 gates[8];
    int gateCount;
    Vector3 loot[4];
    int lootCount;
} SitePlan;

typedef struct {
    Vector3 p;
    int alongZ;
} CurbSpot;

static CityLayout CityLayout_For(WorldMode mode) {
    CityLayout c;
    memset(&c, 0, sizeof(c));
    c.large = (mode == WORLD_RANDOM_256);
    c.cols = c.large ? 4 : 3;
    if (c.large) {
        c.coords[0] = -96.0f; c.coords[1] = -32.0f;
        c.coords[2] = 32.0f;  c.coords[3] = 96.0f;
        c.blockHalf = 25.0f;
        c.siteHalf = 23.0f;
        c.roadHalf = 7.0f;
        c.edgeRoad = 124.5f;
    } else {
        c.coords[0] = -30.0f; c.coords[1] = 0.0f; c.coords[2] = 30.0f;
        c.blockHalf = 11.0f;
        c.siteHalf = 9.2f;
        c.roadHalf = 4.0f;
        c.edgeRoad = 44.5f;
    }
    return c;
}

static int CityRoads(const CityLayout* city, float* out) {
    int n = city->cols - 1;
    for (int i = 0; i < n; i++) out[i] = (city->coords[i] + city->coords[i + 1]) * 0.5f;
    return n;
}

static bool OpenSpot(const WorldMap* map, Vector3 p, float radius,
                     const ReservedSpot* zones, int zoneCount, float clearance) {
    p.y = 0.9f;
    Vector3 half = { radius, 0.9f, radius };
    if (World_MoveCollides(p, half, map->boxes, map->boxCount)) return false;
    if (!PointClearOfReserved(p.x, p.z, zones, zoneCount, clearance)) return false;
    if (fabsf(p.x) < 3.0f && fabsf(p.z) < 3.0f) return false;
    return true;
}

// ---- site plan primitives -------------------------------------------------

static void SiteAdd(SitePlan* s, Vector3 center, Vector3 half, Color color) {
    if (s->n >= (int)(sizeof(s->boxes) / sizeof(s->boxes[0]))) return;
    s->boxes[s->n++] = (Box){ center, half, color };
}

static void SiteGate(SitePlan* s, Vector3 p) {
    if (s->gateCount >= (int)(sizeof(s->gates) / sizeof(s->gates[0]))) return;
    s->gates[s->gateCount++] = p;
}

static void SiteLoot(SitePlan* s, Vector3 p) {
    if (s->lootCount >= (int)(sizeof(s->loot) / sizeof(s->loot[0]))) return;
    p.y = 1.0f;
    s->loot[s->lootCount++] = p;
}

static void SiteWallX(SitePlan* s, float x0, float x1, float z, float t, float h, Color c) {
    if (x1 - x0 < 0.35f) return;
    SiteAdd(s, (Vector3){ (x0 + x1) * 0.5f, h * 0.5f, z },
            (Vector3){ (x1 - x0) * 0.5f, h * 0.5f, t * 0.5f }, c);
}

static void SiteWallZ(SitePlan* s, float x, float z0, float z1, float t, float h, Color c) {
    if (z1 - z0 < 0.35f) return;
    SiteAdd(s, (Vector3){ x, h * 0.5f, (z0 + z1) * 0.5f },
            (Vector3){ t * 0.5f, h * 0.5f, (z1 - z0) * 0.5f }, c);
}

// Builds four perimeter walls with optional gate openings. Side 0/1 are the
// -Z/+Z walls (running along X); side 2/3 are the -X/+X walls (running along Z).
static void SiteRect(SitePlan* s, Vector3 c, float hx, float hz, float t, float h,
                     int gateMask, float gateW, Color col) {
    for (int side = 0; side < 4; side++) {
        bool alongX = (side < 2);
        float a = alongX ? c.x - hx : c.z - hz + t;
        float b = alongX ? c.x + hx : c.z + hz - t;
        if (b - a < 0.4f) continue;
        float fixed = alongX
            ? c.z + ((side == 0) ? -hz : hz)
            : c.x + ((side == 2) ? -hx : hx);
        if (gateMask & (1 << side)) {
            float span = b - a;
            float gw = gateW;
            if (gw > span * 0.78f) gw = span * 0.78f;
            float gc = (a + b) * 0.5f + RngF(-span * 0.16f, span * 0.16f);
            if (alongX) {
                SiteWallX(s, a, gc - gw * 0.5f, fixed, t, h, col);
                SiteWallX(s, gc + gw * 0.5f, b, fixed, t, h, col);
                SiteGate(s, (Vector3){ gc, 0.0f, fixed });
            } else {
                SiteWallZ(s, fixed, a, gc - gw * 0.5f, t, h, col);
                SiteWallZ(s, fixed, gc + gw * 0.5f, b, t, h, col);
                SiteGate(s, (Vector3){ fixed, 0.0f, gc });
            }
        } else if (alongX) {
            SiteWallX(s, a, b, fixed, t, h, col);
        } else {
            SiteWallZ(s, fixed, a, b, t, h, col);
        }
    }
}

static void SiteCrateStack(SitePlan* s, Vector3 at, bool alongX, Color col) {
    const float half = 0.75f;
    const float gap = 0.02f;
    int count = RngI(1, 2);
    for (int i = 0; i < count; i++) {
        float off = (count == 1) ? 0.0f : (i == 0 ? -1.0f : 1.0f) * (half + gap * 0.5f);
        SiteAdd(s, (Vector3){ at.x + (alongX ? off : 0.0f), half,
                              at.z + (alongX ? 0.0f : off) },
                (Vector3){ half, half, half }, col);
    }
    if (RngI(0, 1) == 0) {
        SiteAdd(s, (Vector3){ at.x, half * 3.0f + gap, at.z },
                (Vector3){ half, half, half }, col);
    }
}

static void SiteCompound(SitePlan* s, Vector3 c, float H, bool large) {
    Color wall = (Color){ 118, 112, 102, 255 };
    Color wall2 = (Color){ 139, 133, 122, 255 };
    Color wood = (Color){ 118, 78, 46, 255 };
    float h = H * RngF(0.84f, 0.96f);
    float wh = large ? RngF(3.5f, 4.4f) : RngF(3.0f, 3.8f);
    int gateSide = RngI(0, 3);
    int gateMask = (1 << gateSide) | (1 << ((gateSide + 2) & 3));
    float gw = large ? RngF(6.4f, 7.8f) : RngF(4.6f, 5.6f);
    SiteRect(s, c, h, h, 0.72f, wh, gateMask, gw, wall);
    bool gatesAlongZ = (gateSide < 2);
    // A small solid guardhouse gives compounds a building silhouette and a
    // hard piece of interior cover instead of leaving a bare walled yard.
    float ghx = large ? 2.2f : 1.5f;
    float ghz = large ? 1.8f : 1.3f;
    float ghh = large ? 1.6f : 1.25f;
    float gx = gatesAlongZ ? c.x + h * 0.70f : c.x - h * 0.70f;
    float gz = gatesAlongZ ? c.z + h * 0.70f : c.z - h * 0.70f;
    SiteAdd(s, (Vector3){ gx, ghh, gz }, (Vector3){ ghx, ghh, ghz }, wall2);
    SiteAdd(s, (Vector3){ gx, ghh * 2.0f + 0.11f, gz },
            (Vector3){ ghx + 0.18f, 0.07f, ghz + 0.18f }, wall);
    if (gatesAlongZ) {
        SiteCrateStack(s, (Vector3){ c.x - h * 0.55f, 0.0f, c.z - h * 0.50f },
                       false, wood);
        SiteCrateStack(s, (Vector3){ c.x + h * 0.55f, 0.0f, c.z + h * 0.25f },
                       true, wood);
        SiteAdd(s, (Vector3){ c.x + h * 0.55f, 0.45f, c.z - h * 0.62f },
                (Vector3){ h * 0.22f, 0.45f, 0.45f }, wall2);
        SiteLoot(s, (Vector3){ c.x - h * 0.56f, 0.0f, c.z + h * 0.55f });
        SiteLoot(s, (Vector3){ c.x + h * 0.56f, 0.0f, c.z });
    } else {
        SiteCrateStack(s, (Vector3){ c.x - h * 0.25f, 0.0f, c.z - h * 0.55f },
                       true, wood);
        SiteCrateStack(s, (Vector3){ c.x + h * 0.25f, 0.0f, c.z + h * 0.55f },
                       false, wood);
        SiteAdd(s, (Vector3){ c.x - h * 0.55f, 0.45f, c.z + h * 0.55f },
                (Vector3){ 0.45f, 0.45f, h * 0.22f }, wall2);
        SiteLoot(s, (Vector3){ c.x + h * 0.55f, 0.0f, c.z - h * 0.56f });
        SiteLoot(s, (Vector3){ c.x, 0.0f, c.z + h * 0.56f });
    }
}

static void SiteWarehouse(SitePlan* s, Vector3 c, float H, bool large) {
    Color wall = (Color){ 93, 99, 104, 255 };
    Color wall2 = (Color){ 116, 120, 124, 255 };
    Color wood = (Color){ 122, 82, 48, 255 };
    float hx = H * RngF(0.86f, 0.96f);
    float hz = H * RngF(0.66f, 0.82f);
    if (RngI(0, 1)) { float t = hx; hx = hz; hz = t; }
    float wh = large ? RngF(4.2f, 5.2f) : RngF(3.5f, 4.2f);
    int gateSide = RngI(0, 3);
    int gateMask = (1 << gateSide) | (1 << ((gateSide + 2) & 3));
    float gw = large ? RngF(6.0f, 7.4f) : RngF(4.2f, 5.2f);
    SiteRect(s, c, hx, hz, 0.80f, wh, gateMask, gw, wall);
    bool gatesAlongZ = (gateSide < 2);
    if (gatesAlongZ) {
        SiteCrateStack(s, (Vector3){ c.x - hx * 0.55f, 0.0f, c.z - hz * 0.34f },
                       false, wood);
        SiteCrateStack(s, (Vector3){ c.x + hx * 0.55f, 0.0f, c.z + hz * 0.34f },
                       true, wood);
        SiteCrateStack(s, (Vector3){ c.x - hx * 0.55f, 0.0f, c.z + hz * 0.34f },
                       false, wood);
        SiteAdd(s, (Vector3){ c.x + hx * 0.86f, 0.6f, c.z },
                (Vector3){ 0.34f, 0.6f, hz * 0.30f }, wall2);
        SiteLoot(s, (Vector3){ c.x, 0.0f, c.z });
        SiteLoot(s, (Vector3){ c.x, 0.0f, c.z + hz * 0.55f });
    } else {
        SiteCrateStack(s, (Vector3){ c.x - hx * 0.34f, 0.0f, c.z - hz * 0.55f },
                       true, wood);
        SiteCrateStack(s, (Vector3){ c.x + hx * 0.34f, 0.0f, c.z + hz * 0.55f },
                       false, wood);
        SiteCrateStack(s, (Vector3){ c.x + hx * 0.34f, 0.0f, c.z - hz * 0.55f },
                       true, wood);
        SiteAdd(s, (Vector3){ c.x, 0.6f, c.z + hz * 0.86f },
                (Vector3){ hx * 0.30f, 0.6f, 0.32f }, wall2);
        SiteLoot(s, (Vector3){ c.x, 0.0f, c.z });
        SiteLoot(s, (Vector3){ c.x + hx * 0.55f, 0.0f, c.z });
    }
}

static void SiteContainerYard(SitePlan* s, Vector3 c, float H, bool large) {
    Color cols[4] = {
        { 46, 79, 104, 255 }, { 118, 54, 44, 255 },
        { 54, 86, 62, 255 }, { 119, 94, 48, 255 },
    };
    float hx = H * 0.90f;
    float hz = H * 0.90f;
    bool alongX = RngI(0, 1) != 0;
    float len = large ? RngF(7.0f, 9.0f) : RngF(3.6f, 4.6f);
    float wid = large ? 1.30f : 1.05f;
    float hgt = large ? 1.25f : 1.05f;
    float dLong = large ? hx * 0.59f : hx * 0.58f;
    float dSide = large ? hz * 0.50f : hz * 0.49f;
    for (int row = -1; row <= 1; row += 2) {
        for (int col = -1; col <= 1; col++) {
            float ox = alongX ? (float)col * dLong : (float)row * dSide;
            float oz = alongX ? (float)row * dSide : (float)col * dLong;
            // Leave one side of the central loading square open. The old
            // version boxed it in with containers and crates, which looked
            // fine from above but made centre loot unreachable on foot.
            if (fabsf(ox) < 0.1f && oz > 0.0f) continue;
            Vector3 half = alongX ? (Vector3){ len * 0.5f, hgt, wid }
                                  : (Vector3){ wid, hgt, len * 0.5f };
            Color colr = cols[((row + 1) + (col + 1) * 2) % 4];
            SiteAdd(s, (Vector3){ c.x + ox, hgt, c.z + oz }, half, colr);
            if (col == 0 && row == -1) {
                SiteAdd(s, (Vector3){ c.x + ox, hgt * 3.0f + 0.03f, c.z + oz },
                        half, colr);
            }
        }
    }
    SiteCrateStack(s, (Vector3){ c.x - hx * 0.88f, 0.0f, c.z - hz * 0.86f },
                   true, (Color){ 120, 80, 48, 255 });
    SiteCrateStack(s, (Vector3){ c.x + hx * 0.88f, 0.0f, c.z + hz * 0.86f },
                   false, (Color){ 120, 80, 48, 255 });
    SiteLoot(s, (Vector3){ c.x, 0.0f, c.z });
    SiteLoot(s, (Vector3){ c.x + (alongX ? 0.0f : dSide * 0.55f), 0.0f,
                            c.z + (alongX ? dSide * 0.55f : 0.0f) });
}

static void SiteRuin(SitePlan* s, Vector3 c, float H, bool large) {
    Color wall = (Color){ 117, 111, 101, 255 };
    Color wall2 = (Color){ 137, 129, 117, 255 };
    float hx = H * RngF(0.82f, 0.94f);
    float hz = H * RngF(0.74f, 0.88f);
    float wh = large ? RngF(3.6f, 4.8f) : RngF(2.8f, 3.7f);
    float t = 0.72f;
    SiteWallX(s, c.x - hx, c.x + hx, c.z - hz, t, wh, wall);
    SiteWallZ(s, c.x + hx, c.z - hz + t, c.z + hz, t, wh, wall);
    float gw = large ? RngF(5.2f, 7.0f) : RngF(3.8f, 4.8f);
    float gc = c.z + RngF(-hz * 0.20f, hz * 0.20f);
    SiteWallZ(s, c.x - hx, c.z - hz + t, gc - gw * 0.5f, t, wh, wall);
    SiteWallZ(s, c.x - hx, gc + gw * 0.5f, c.z + hz, t, wh, wall);
    SiteGate(s, (Vector3){ c.x - hx, 0.0f, gc });
    float sg = large ? 4.4f : 3.2f;
    SiteWallX(s, c.x - hx + t, c.x - sg * 0.5f, c.z + hz, t, 0.75f, wall2);
    SiteWallX(s, c.x + sg * 0.5f, c.x + hx - t, c.z + hz, t, 0.75f, wall2);
    SiteAdd(s, (Vector3){ c.x + hx * 0.42f, 0.42f, c.z - hz * 0.45f },
            (Vector3){ hx * 0.16f, 0.42f, hz * 0.12f }, wall2);
    SiteAdd(s, (Vector3){ c.x - hx * 0.42f, 0.30f, c.z + hz * 0.30f },
            (Vector3){ hx * 0.12f, 0.30f, hz * 0.16f }, wall2);
    SiteLoot(s, (Vector3){ c.x, 0.0f, c.z });
    SiteLoot(s, (Vector3){ c.x - hx * 0.40f, 0.0f, c.z + hz * 0.48f });
}

static void SiteCheckpoint(SitePlan* s, Vector3 c, float H, bool large) {
    Color concrete = (Color){ 126, 121, 111, 255 };
    Color concrete2 = (Color){ 145, 138, 127, 255 };
    Color sand = (Color){ 122, 110, 82, 255 };
    float w = H * 0.84f;
    float gap = large ? 5.4f : 4.2f;
    SiteWallX(s, c.x - w, c.x - gap * 0.5f, c.z - 2.0f, 0.55f, 1.15f, concrete2);
    SiteWallX(s, c.x + gap * 0.5f, c.x + w, c.z - 2.0f, 0.55f, 1.15f, concrete2);
    SiteWallX(s, c.x - w * 0.82f, c.x - gap * 0.65f, c.z + 3.0f, 0.55f, 1.0f, concrete);
    SiteWallX(s, c.x + gap * 0.65f, c.x + w * 0.82f, c.z + 3.0f, 0.55f, 1.0f, concrete);
    SiteAdd(s, (Vector3){ c.x + w * 0.62f, 1.35f, c.z - H * 0.42f },
            (Vector3){ 1.25f, 1.35f, 1.25f }, concrete);
    SiteAdd(s, (Vector3){ c.x - w * 0.55f, 0.32f, c.z + H * 0.20f },
            (Vector3){ 1.45f, 0.32f, 0.55f }, sand);
    SiteCrateStack(s, (Vector3){ c.x - w * 0.62f, 0.0f, c.z - H * 0.38f },
                   true, (Color){ 120, 80, 48, 255 });
    SiteLoot(s, (Vector3){ c.x, 0.0f, c.z + 1.0f });
    SiteLoot(s, (Vector3){ c.x + w * 0.55f, 0.0f, c.z + H * 0.16f });
}

static void SitePlaza(SitePlan* s, Vector3 c, float H, bool large) {
    (void)large;
    Color stone = (Color){ 138, 136, 128, 255 };
    Color stone2 = (Color){ 158, 154, 144, 255 };
    Color green = (Color){ 76, 112, 70, 255 };
    Color wood = (Color){ 108, 78, 50, 255 };
    float hx = H * 0.92f, hz = H * 0.92f;
    SiteAdd(s, (Vector3){ c.x, 0.15f, c.z }, (Vector3){ 1.7f, 0.15f, 1.7f }, stone2);
    SiteAdd(s, (Vector3){ c.x, 1.35f, c.z }, (Vector3){ 0.78f, 1.05f, 0.78f }, stone);
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            SiteAdd(s, (Vector3){ c.x + sx * hx * 0.52f, 0.52f,
                                  c.z + sz * hz * 0.52f },
                    (Vector3){ 1.25f, 0.52f, 0.72f }, green);
        }
    }
    for (int sx = -1; sx <= 1; sx += 2) {
        SiteAdd(s, (Vector3){ c.x + sx * hx * 0.34f, 0.28f, c.z },
                (Vector3){ 0.80f, 0.28f, 0.34f }, wood);
        SiteAdd(s, (Vector3){ c.x, 0.28f, c.z + sx * hz * 0.34f },
                (Vector3){ 0.34f, 0.28f, 0.80f }, wood);
    }
    SiteLoot(s, (Vector3){ c.x + hx * 0.34f, 0.0f, c.z + hz * 0.34f });
    SiteLoot(s, (Vector3){ c.x - hx * 0.34f, 0.0f, c.z - hz * 0.30f });
}

static void SiteMarket(SitePlan* s, Vector3 c, float H, bool large) {
    (void)large;
    Color wood = (Color){ 118, 82, 48, 255 };
    Color fabric[3] = {
        { 142, 60, 52, 255 }, { 50, 82, 116, 255 }, { 58, 100, 70, 255 },
    };
    float hx = H * 0.88f, hz = H * 0.86f;
    int ci = 0;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            float x = c.x + sx * hx * 0.42f;
            float z0 = c.z + sz * hz * 0.40f;
            SiteAdd(s, (Vector3){ x, 1.10f, z0 + sz * 0.55f },
                    (Vector3){ 1.20f, 1.10f, 0.10f }, wood);
            SiteAdd(s, (Vector3){ x, 0.40f, z0 - sz * 0.35f },
                    (Vector3){ 1.20f, 0.40f, 0.35f }, wood);
            SiteAdd(s, (Vector3){ x, 2.31f, z0 },
                    (Vector3){ 1.30f, 0.07f, 0.85f }, fabric[ci % 3]);
            ci++;
        }
    }
    SiteCrateStack(s, (Vector3){ c.x, 0.0f, c.z }, true,
                   (Color){ 120, 80, 48, 255 });
    SiteLoot(s, (Vector3){ c.x + hx * 0.32f, 0.0f, c.z });
    SiteLoot(s, (Vector3){ c.x - hx * 0.32f, 0.0f, c.z });
}

static void SiteConstruction(SitePlan* s, Vector3 c, float H, bool large) {
    (void)large;
    Color concrete = (Color){ 128, 122, 111, 255 };
    Color steel = (Color){ 91, 97, 103, 255 };
    float hx = H * 0.88f, hz = H * 0.86f;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            SiteAdd(s, (Vector3){ c.x + sx * hx * 0.62f, 2.20f,
                                  c.z + sz * hz * 0.62f },
                    (Vector3){ 0.30f, 2.20f, 0.30f }, concrete);
        }
    }
    for (int sz = -1; sz <= 1; sz += 2) {
        SiteWallX(s, c.x - hx * 0.46f, c.x + hx * 0.46f,
                  c.z + sz * hz * 0.85f, 0.32f, 0.90f, concrete);
    }
    for (int sz = -1; sz <= 1; sz += 2) {
        SiteAdd(s, (Vector3){ c.x, 4.32f, c.z + sz * hz * 0.62f },
                (Vector3){ hx * 0.62f - 0.36f, 0.18f, 0.24f }, steel);
    }
    SiteCrateStack(s, (Vector3){ c.x - hx * 0.18f, 0.0f, c.z + hz * 0.48f },
                   false, (Color){ 124, 84, 50, 255 });
    SiteAdd(s, (Vector3){ c.x + hx * 0.36f, 0.18f, c.z - hz * 0.20f },
            (Vector3){ hx * 0.20f, 0.18f, 0.18f }, steel);
    SiteAdd(s, (Vector3){ c.x - hx * 0.48f, 0.55f, c.z - hz * 0.55f },
            (Vector3){ 0.55f, 0.55f, 1.25f }, concrete);
    SiteLoot(s, (Vector3){ c.x, 0.0f, c.z });
    SiteLoot(s, (Vector3){ c.x + hx * 0.35f, 0.0f, c.z + hz * 0.42f });
}

static void SiteCompact(SitePlan* s, Vector3 c, float H) {
    Color stone = (Color){ 138, 136, 128, 255 };
    Color green = (Color){ 76, 112, 70, 255 };
    float r = H * 0.46f;
    SiteAdd(s, (Vector3){ c.x, 1.15f, c.z }, (Vector3){ 0.80f, 1.15f, 0.80f }, stone);
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            SiteAdd(s, (Vector3){ c.x + sx * r, 0.50f, c.z + sz * r },
                    (Vector3){ 1.15f, 0.50f, 0.70f }, green);
        }
    }
    SiteLoot(s, (Vector3){ c.x + r, 0.0f, c.z });
    SiteLoot(s, (Vector3){ c.x - r, 0.0f, c.z });
}

static int SiteKindFor(bool large, int prev) {
    static const unsigned char tableLarge[16] = {
        0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7,
    };
    static const unsigned char tableSmall[16] = {
        0, 0, 0, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 7,
    };
    int kind = large ? tableLarge[RngI(0, 15)] : tableSmall[RngI(0, 15)];
    if (kind == prev && RngI(0, 99) < 70) {
        kind = (kind + 1 + RngI(0, 6)) % 8;
    }
    return kind;
}

static void BuildSite(SitePlan* s, Vector3 c, float H, int kind, bool large) {
    memset(s, 0, sizeof(*s));
    switch (kind) {
        case 0: SiteCompound(s, c, H, large); break;
        case 1: SiteWarehouse(s, c, H, large); break;
        case 2: SiteContainerYard(s, c, H, large); break;
        case 3: SiteRuin(s, c, H, large); break;
        case 4: SiteCheckpoint(s, c, H, large); break;
        case 5: SitePlaza(s, c, H, large); break;
        case 6: SiteMarket(s, c, H, large); break;
        case 7: SiteConstruction(s, c, H, large); break;
        default: SiteCompact(s, c, H); break;
    }
}

// ---- road-side furniture --------------------------------------------------

static void CurbAdd(CurbSpot* spots, int* count, int cap, float x, float z, int alongZ) {
    if (*count >= cap) return;
    spots[*count].p = (Vector3){ x, 0.0f, z };
    spots[*count].alongZ = alongZ;
    (*count)++;
}

static int CollectCurbSpots(const WorldMap* map, const CityLayout* city,
                            CurbSpot* out, int cap) {
    float roads[4];
    int rn = CityRoads(city, roads);
    int n = 0;
    float step = city->large ? 9.0f : 7.0f;
    float lo = -map->limit + 10.0f;
    float hi = map->limit - 10.0f;
    float clear = city->roadHalf + 4.2f;
    for (int r = 0; r < rn; r++) {
        for (int side = -1; side <= 1; side += 2) {
            float px = roads[r] + (float)side * (city->roadHalf - 1.0f);
            for (float z = lo; z <= hi; z += step) {
                bool near = false;
                for (int k = 0; k < rn; k++) {
                    if (fabsf(z - roads[k]) < clear) { near = true; break; }
                }
                if (!near) CurbAdd(out, &n, cap, px, z, 1);
            }
            float pz = roads[r] + (float)side * (city->roadHalf - 1.0f);
            for (float x = lo; x <= hi; x += step) {
                bool near = false;
                for (int k = 0; k < rn; k++) {
                    if (fabsf(x - roads[k]) < clear) { near = true; break; }
                }
                if (!near) CurbAdd(out, &n, cap, x, pz, 0);
            }
        }
    }
    // A perimeter road ring makes the outermost blocks read as a real city
    // edge rather than a wall sitting directly behind the buildings.
    float edge = city->edgeRoad;
    for (int sx = -1; sx <= 1; sx += 2) {
        float px = (float)sx * edge - (float)sx * 1.0f;
        for (float z = lo; z <= hi; z += step) {
            bool near = false;
            for (int k = 0; k < rn; k++) {
                if (fabsf(z - roads[k]) < clear) { near = true; break; }
            }
            if (!near) CurbAdd(out, &n, cap, px, z, 1);
        }
    }
    for (int sz = -1; sz <= 1; sz += 2) {
        float pz = (float)sz * edge - (float)sz * 1.0f;
        for (float x = lo; x <= hi; x += step) {
            bool near = false;
            for (int k = 0; k < rn; k++) {
                if (fabsf(x - roads[k]) < clear) { near = true; break; }
            }
            if (!near) CurbAdd(out, &n, cap, x, pz, 0);
        }
    }
    return n;
}

static int BuildStreetProp(Box* out, CurbSpot spot, int type) {
    Color concrete2 = (Color){ 146, 140, 129, 255 };
    Color wood = (Color){ 120, 80, 48, 255 };
    Color red = (Color){ 118, 53, 45, 255 };
    Color green = (Color){ 54, 86, 60, 255 };
    Color dark = (Color){ 53, 55, 60, 255 };
    Color carCols[4] = {
        { 104, 106, 110, 255 }, { 77, 86, 97, 255 },
        { 105, 76, 66, 255 }, { 68, 88, 75, 255 },
    };
    bool az = spot.alongZ != 0;
    int n = 0;
    if (type == 0) {
        float len = RngF(3.2f, 5.0f) * 0.5f;
        float h = RngF(1.0f, 1.35f);
        out[n++] = (Box){ { spot.p.x, h * 0.5f, spot.p.z },
                         { az ? 0.24f : len, h * 0.5f, az ? len : 0.24f },
                         concrete2 };
    } else if (type == 1) {
        float half = 0.75f;
        int count = RngI(1, 2);
        for (int i = 0; i < count; i++) {
            float off = (count == 1) ? 0.0f : (i == 0 ? -0.76f : 0.76f);
            out[n++] = (Box){ { spot.p.x + (az ? 0.0f : off), half,
                                spot.p.z + (az ? off : 0.0f) },
                              { half, half, half }, wood };
        }
        if (RngI(0, 1) == 0) {
            out[n++] = (Box){ { spot.p.x, half * 3.0f + 0.02f, spot.p.z },
                              { half, half, half }, wood };
        }
    } else if (type == 2) {
        out[n++] = (Box){ { spot.p.x, 0.50f, spot.p.z },
                         { az ? 0.50f : 0.95f, 0.50f, az ? 0.95f : 0.50f },
                         green };
        out[n++] = (Box){ { spot.p.x, 1.06f, spot.p.z },
                         { az ? 0.53f : 1.00f, 0.06f, az ? 1.00f : 0.53f },
                         dark };
    } else if (type == 3) {
        Color c = carCols[RngI(0, 3)];
        out[n++] = (Box){ { spot.p.x, 0.55f, spot.p.z },
                         { az ? 0.85f : 2.0f, 0.55f, az ? 2.0f : 0.85f }, c };
        out[n++] = (Box){ { spot.p.x, 1.32f, spot.p.z },
                         { az ? 0.75f : 1.0f, 0.22f, az ? 1.0f : 0.75f }, dark };
    } else if (type == 4) {
        out[n++] = (Box){ { spot.p.x, 0.32f, spot.p.z },
                         { az ? 0.42f : 1.35f, 0.32f, az ? 1.35f : 0.42f },
                         (Color){ 122, 110, 82, 255 } };
    } else {
        out[n++] = (Box){ { spot.p.x, 0.62f, spot.p.z },
                         { az ? 0.38f : 1.8f, 0.62f, az ? 1.8f : 0.38f },
                         red };
    }
    return n;
}

static void ShuffleCurbs(CurbSpot* spots, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = RngI(0, i);
        CurbSpot t = spots[i]; spots[i] = spots[j]; spots[j] = t;
    }
}

// ---- populating the city --------------------------------------------------

static int GenerateSites(WorldMap* map, const CityLayout* city,
                         ReservedSpot* zones, int* zoneCount,
                         Vector3* siteLoot, int* siteLootCount, int lootCap) {
    int grid = city->cols;
    int placed = 0;
    int prevKind = -1;
    map->siteCount = grid * grid;
    for (int gz = 0; gz < grid; gz++) {
        for (int gx = 0; gx < grid; gx++) {
            int idx = gz * grid + gx;
            map->siteKinds[idx] = 255;
            float jitter = city->large ? 0.9f : 0.35f;
            Vector3 c = { city->coords[gx] + RngF(-jitter, jitter), 0.0f,
                          city->coords[gz] + RngF(-jitter, jitter) };
            int kind = SiteKindFor(city->large, prevKind);
            bool done = false;
            for (int attempt = 0; attempt < 4 && !done; attempt++) {
                SitePlan plan;
                int useKind = (attempt == 0) ? kind : (kind + attempt * 2 + 1) % 8;
                BuildSite(&plan, c, city->siteHalf, useKind, city->large);
                if (plan.n < 2) continue;
                if (!CoverCandidateOk(map, plan.boxes, plan.n, zones, *zoneCount)) continue;
                for (int i = 0; i < plan.n; i++) AddBox(map, plan.boxes[i]);
                float gateR = city->large ? 4.2f : 3.0f;
                for (int i = 0; i < plan.gateCount; i++) {
                    if (*zoneCount < 126) {
                        zones[*zoneCount] = (ReservedSpot){ plan.gates[i].x,
                                                            plan.gates[i].z,
                                                            gateR };
                        (*zoneCount)++;
                    }
                }
                for (int i = 0; i < plan.lootCount && *siteLootCount < lootCap; i++) {
                    if (OpenSpot(map, plan.loot[i], 0.32f, zones, *zoneCount, 0.45f)) {
                        siteLoot[*siteLootCount] = plan.loot[i];
                        (*siteLootCount)++;
                    }
                }
                map->siteKinds[idx] = (unsigned char)useKind;
                prevKind = useKind;
                placed++;
                done = true;
            }
            if (!done) {
                SitePlan plan;
                BuildSite(&plan, c, city->siteHalf, 8, city->large);
                if (plan.n >= 2 && CoverCandidateOk(map, plan.boxes, plan.n, zones, *zoneCount)) {
                    for (int i = 0; i < plan.n; i++) AddBox(map, plan.boxes[i]);
                    for (int i = 0; i < plan.lootCount && *siteLootCount < lootCap; i++) {
                        if (OpenSpot(map, plan.loot[i], 0.32f, zones, *zoneCount, 0.45f)) {
                            siteLoot[*siteLootCount] = plan.loot[i];
                            (*siteLootCount)++;
                        }
                    }
                    map->siteKinds[idx] = 8;
                    placed++;
                }
            }
        }
    }
    return placed;
}

static void GenerateStreetProps(WorldMap* map, const CityLayout* city,
                                const ReservedSpot* zones, int zoneCount,
                                CurbSpot* curbs, int curbCount) {
    int target = city->large ? 30 : 14;
    int placed = 0;
    for (int i = 0; i < curbCount && placed < target; i++) {
        int type = RngI(0, 4);
        Box cand[4];
        int n = BuildStreetProp(cand, curbs[i], type);
        if (!CoverCandidateOk(map, cand, n, zones, zoneCount)) continue;
        for (int k = 0; k < n; k++) AddBox(map, cand[k]);
        placed++;
    }
}

static void PlaceLamps(WorldMap* map, CityLayout* city,
                       const ReservedSpot* zones, int zoneCount,
                       CurbSpot* curbs, int curbCount) {
    int target = WORLD_MAX_STREET_LAMPS;
    Vector3 bases[WORLD_MAX_STREET_LAMPS] = { 0 };
    int start = curbCount > 0 ? RngI(0, curbCount - 1) : 0;
    for (int i = 0; i < curbCount && map->streetLampCount < target; i++) {
        CurbSpot s = curbs[(start + i) % curbCount];
        if (!OpenSpot(map, s.p, 0.30f, zones, zoneCount, 1.4f)) continue;
        float minD = city->large ? 30.0f : 15.0f;
        bool far = true;
        for (int k = 0; k < map->streetLampCount; k++) {
            float dx = s.p.x - bases[k].x, dz = s.p.z - bases[k].z;
            if (dx * dx + dz * dz < minD * minD) { far = false; break; }
        }
        if (!far) continue;
        int id = map->streetLampCount;
        bases[id] = s.p;
        map->streetLamps[id] = (Vector3){ s.p.x, 4.6f, s.p.z };
        map->streetLampCount++;
    }
}

static void PlaceBarrels(WorldMap* map, const CityLayout* city,
                         ReservedSpot* zones, int* zoneCount,
                         const Vector3* siteLoot, int siteLootCount,
                         CurbSpot* curbs, int curbCount) {
    int target = MAX_BARRELS;
    int placed = 0;
    float minD = city->large ? 18.0f : 9.0f;
    for (int attempt = 0; attempt < 180 && placed < target; attempt++) {
        Vector3 p;
        bool useSite = (siteLootCount > 0) && (RngI(0, 2) != 0);
        if (useSite) {
            p = siteLoot[RngI(0, siteLootCount - 1)];
        } else if (curbCount > 0) {
            p = curbs[RngI(0, curbCount - 1)].p;
        } else {
            continue;
        }
        if (!OpenSpot(map, p, 0.48f, zones, *zoneCount, 1.5f)) continue;
        bool far = true;
        for (int i = 0; i < placed; i++) {
            float dx = p.x - map->barrelSpots[i].x;
            float dz = p.z - map->barrelSpots[i].z;
            if (dx * dx + dz * dz < minD * minD) { far = false; break; }
        }
        if (!far) continue;
        p.y = 0.0f;
        map->barrelSpots[placed] = p;
        placed++;
        Reserve(zones, zoneCount, p, 2.4f);
    }
    map->barrelCount = placed;
}

static float PickupMinDistance(const CityLayout* city) {
    return city->large ? 11.0f : 6.0f;
}

static void PlacePickups(WorldMap* map, const CityLayout* city,
                         ReservedSpot* zones, int* zoneCount,
                         const Vector3* siteLoot, int siteLootCount,
                         CurbSpot* curbs, int curbCount) {
    int target = city->large ? 20 : 8;
    float minD = PickupMinDistance(city);
    int siteOrder[WORLD_MAX_SITES * 2];
    int siteOrderCount = siteLootCount;
    if (siteOrderCount > (int)(sizeof(siteOrder) / sizeof(siteOrder[0]))) {
        siteOrderCount = (int)(sizeof(siteOrder) / sizeof(siteOrder[0]));
    }
    for (int i = 0; i < siteOrderCount; i++) siteOrder[i] = i;
    for (int i = siteOrderCount - 1; i > 0; i--) {
        int j = RngI(0, i);
        int t = siteOrder[i]; siteOrder[i] = siteOrder[j]; siteOrder[j] = t;
    }
    int siteCursor = 0;
    int roadCursor = curbCount > 0 ? RngI(0, curbCount - 1) : 0;
    for (int i = 0; i < target; i++) {
        bool got = false;
        for (int tries = 0; tries < 8 && !got; tries++) {
            Vector3 p;
            bool wantSite = (i % 3) != 2;
            if (wantSite && siteCursor < siteOrderCount) {
                p = siteLoot[siteOrder[siteCursor++]];
            } else if (curbCount > 0) {
                p = curbs[roadCursor % curbCount].p;
                roadCursor++;
            } else if (siteCursor < siteOrderCount) {
                p = siteLoot[siteOrder[siteCursor++]];
            } else {
                break;
            }
            if (!OpenSpot(map, p, 0.35f, zones, *zoneCount, 1.5f)) continue;
            bool far = true;
            for (int k = 0; k < map->pickupCount; k++) {
                float dx = p.x - map->pickupSpots[k].x;
                float dz = p.z - map->pickupSpots[k].z;
                if (dx * dx + dz * dz < minD * minD) { far = false; break; }
            }
            if (!far) continue;
            p.y = 1.0f;
            map->pickupSpots[map->pickupCount] = p;
            map->pickupTypes[map->pickupCount] =
                ((i % 4) == 0) ? PICKUP_HEALTH : PICKUP_AMMO;
            map->pickupCount++;
            Reserve(zones, zoneCount, p, 2.3f);
            got = true;
        }
        if (!got) {
            float margin = 5.0f;
            for (int attempt = 0; attempt < 700 && !got; attempt++) {
                float x = RngF(-map->limit + margin, map->limit - margin);
                float z = RngF(-map->limit + margin, map->limit - margin);
                Vector3 p = { x, 0.0f, z };
                if (!OpenSpot(map, p, 0.35f, zones, *zoneCount, 1.2f)) continue;
                bool far = true;
                for (int k = 0; k < map->pickupCount; k++) {
                    float dx = p.x - map->pickupSpots[k].x;
                    float dz = p.z - map->pickupSpots[k].z;
                    if (dx * dx + dz * dz < minD * minD) { far = false; break; }
                }
                if (!far) continue;
                p.y = 1.0f;
                map->pickupSpots[map->pickupCount] = p;
                map->pickupTypes[map->pickupCount] =
                    ((i % 4) == 0) ? PICKUP_HEALTH : PICKUP_AMMO;
                map->pickupCount++;
                Reserve(zones, zoneCount, p, 2.3f);
                got = true;
            }
        }
    }
}

static void SafeOpenFallback(WorldMap* map, const ReservedSpot* zones, int zoneCount) {
    map->boxCount = 4;
    const int pieces = 14;
    for (int i = 0; i < pieces; i++) {
        float a = (float)i * (2.0f * PI / (float)pieces) + 0.17f;
        float r = map->limit * (0.34f + 0.10f * (float)(i & 1));
        CurbSpot spot;
        spot.p = (Vector3){ cosf(a) * r, 0.0f, sinf(a) * r };
        spot.alongZ = (i & 1);
        Box cand[4];
        int n = BuildStreetProp(cand, spot, (i % 3 == 0) ? 3 : ((i % 3 == 1) ? 1 : 4));
        if (!CoverCandidateOk(map, cand, n, zones, zoneCount)) continue;
        for (int k = 0; k < n; k++) AddBox(map, cand[k]);
    }
}

static bool City_OnRoad(const WorldMap* map, float x, float z) {
    if (map->cityCols <= 0) return false;
    float r = map->cityRoadHalf + 0.45f;
    for (int i = 0; i + 1 < map->cityCols; i++) {
        float road = (map->cityCoords[i] + map->cityCoords[i + 1]) * 0.5f;
        if (fabsf(x - road) < r || fabsf(z - road) < r) return true;
    }
    float edge = map->cityEdgeRoad;
    if (fabsf(fabsf(x) - edge) < 4.0f || fabsf(fabsf(z) - edge) < 4.0f) return true;
    return false;
}

static bool City_NearLoot(const WorldMap* map, float x, float z, float radius) {
    for (int i = 0; i < map->pickupCount; i++) {
        float dx = x - map->pickupSpots[i].x, dz = z - map->pickupSpots[i].z;
        if (dx * dx + dz * dz < radius * radius) return true;
    }
    for (int i = 0; i < map->barrelCount; i++) {
        float dx = x - map->barrelSpots[i].x, dz = z - map->barrelSpots[i].z;
        if (dx * dx + dz * dz < radius * radius) return true;
    }
    for (int i = 0; i < map->streetLampCount; i++) {
        float dx = x - map->streetLamps[i].x, dz = z - map->streetLamps[i].z;
        if (dx * dx + dz * dz < radius * radius) return true;
    }
    return false;
}

static void AddRandomDecor(WorldMap* map) {
    int grassTarget = (map->mode == WORLD_RANDOM_256) ? 120 : 80;
    int rockTarget  = (map->mode == WORLD_RANDOM_256) ? 15 : 9;
    int pebTarget   = (map->mode == WORLD_RANDOM_256) ? 100 : 64;
    float inner = map->limit - 2.0f;

    for (int i = 0; i < grassTarget * 3 && map->grassCount < grassTarget; i++) {
        float x = RngF(-inner, inner), z = RngF(-inner, inner);
        if (City_OnRoad(map, x, z) || City_NearLoot(map, x, z, 1.2f)) continue;
        Vector3 p = { x, 0.16f, z };
        Vector3 half = { 0.08f, 0.10f, 0.08f };
        if (World_MoveCollides(p, half, map->boxes, map->boxCount)) continue;
        map->grass[map->grassCount] = p;
        map->grassSize[map->grassCount] = RngF(0.09f, 0.18f);
        map->grassCount++;
    }
    for (int i = 0; i < rockTarget * 6 && map->rockCount < rockTarget; i++) {
        float x = RngF(-inner, inner), z = RngF(-inner, inner);
        if (City_OnRoad(map, x, z) || City_NearLoot(map, x, z, 1.6f)) continue;
        Vector3 p = { x, 0.0f, z };
        Vector3 half = { 0.5f, 0.5f, 0.5f };
        if (World_MoveCollides(p, half, map->boxes, map->boxCount)) continue;
        map->rocks[map->rockCount] = p;
        map->rockSize[map->rockCount] = RngF(0.45f, 0.9f);
        map->rockCount++;
    }
    for (int i = 0; i < pebTarget * 3 && map->pebbleCount < pebTarget; i++) {
        float x = RngF(-inner, inner), z = RngF(-inner, inner);
        Vector3 p = { x, 0.09f, z };
        Vector3 half = { 0.06f, 0.06f, 0.06f };
        if (World_MoveCollides(p, half, map->boxes, map->boxCount)) continue;
        map->pebbles[map->pebbleCount] = p;
        map->pebbleSize[map->pebbleCount] = RngF(0.06f, 0.16f);
        map->pebbleCount++;
    }
}

static void World_Random_Attempt(WorldMap* map, WorldMode mode, uint32_t seed, int retry) {
    memset(map, 0, sizeof(*map));
    map->mode = mode;
    map->seed = seed ? seed : 1u;
    map->limit = (mode == WORLD_RANDOM_256) ? 128.0f : 48.0f;
    map->groundSize = map->limit * 2.0f + 24.0f;
    CityLayout city = CityLayout_For(mode);
    map->cityCols = city.cols;
    for (int i = 0; i < city.cols; i++) map->cityCoords[i] = city.coords[i];
    map->cityBlockHalf = city.blockHalf;
    map->cityRoadHalf = city.roadHalf;
    map->cityEdgeRoad = city.edgeRoad;
    s_rng = map->seed;
    (void)RngU32();

    // Sealed boundary. Sites stay inside their blocks, so this wall is just
    // the final playable edge and never doubles as level geometry.
    float wallH = 3.2f;
    float L = map->limit;
    AddBox(map, (Box){ { 0, wallH, -L }, { L, wallH, 0.5f }, (Color){ 120, 115, 105, 255 } });
    AddBox(map, (Box){ { 0, wallH,  L }, { L, wallH, 0.5f }, (Color){ 120, 115, 105, 255 } });
    AddBox(map, (Box){ {  L, wallH, 0 }, { 0.5f, wallH, L }, (Color){ 120, 115, 105, 255 } });
    AddBox(map, (Box){ { -L, wallH, 0 }, { 0.5f, wallH, L }, (Color){ 120, 115, 105, 255 } });

    if (mode == WORLD_RANDOM_256) {
        Vector3 players[WORLD_MAX_PLAYER_SPAWNS] = {
            { -64, 0.0f, -64 }, { 64, 0.0f, -64 }, { 64, 0.0f, 64 }, { -64, 0.0f, 64 },
        };
        for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++) map->playerSpawns[i] = players[i];
        Vector3 enemies[MAX_ENEMIES] = {
            { -64, 0.9f, -32 }, { 64, 0.9f, 32 }, { 32, 0.9f, -64 },
            { -32, 0.9f, 64 }, { 0, 0.9f, 0 }, { 0, 0.9f, 96 },
        };
        for (int i = 0; i < MAX_ENEMIES; i++) map->enemySpawns[i] = enemies[i];
    } else {
        Vector3 players[WORLD_MAX_PLAYER_SPAWNS] = {
            { 0, 0.0f, 45 }, { 45, 0.0f, 0 }, { 0, 0.0f, -45 }, { -45, 0.0f, 0 },
        };
        for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++) map->playerSpawns[i] = players[i];
        Vector3 enemies[MAX_ENEMIES] = {
            { -15, 0.9f, -30 }, { 15, 0.9f, 30 }, { 30, 0.9f, 15 },
            { -30, 0.9f, -15 }, { 15, 0.9f, -15 }, { -15, 0.9f, 15 },
        };
        for (int i = 0; i < MAX_ENEMIES; i++) map->enemySpawns[i] = enemies[i];
    }
    map->enemySpawnCount = MAX_ENEMIES;

    ReservedSpot zones[128];
    int zc = 0;
    for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++)
        Reserve(zones, &zc, map->playerSpawns[i], 5.0f);
    for (int i = 0; i < map->enemySpawnCount; i++)
        Reserve(zones, &zc, map->enemySpawns[i], 3.6f);

    CurbSpot curbs[512];
    int curbCount = CollectCurbSpots(map, &city, curbs, 512);
    ShuffleCurbs(curbs, curbCount);

    Vector3 siteLoot[WORLD_MAX_SITES * 2];
    int siteLootCount = 0;
    int sites = GenerateSites(map, &city, zones, &zc, siteLoot, &siteLootCount,
                              (int)(sizeof(siteLoot) / sizeof(siteLoot[0])));
    GenerateStreetProps(map, &city, zones, zc, curbs, curbCount);
    PlaceLamps(map, &city, zones, zc, curbs, curbCount);
    PlaceBarrels(map, &city, zones, &zc, siteLoot, siteLootCount, curbs, curbCount);
    PlacePickups(map, &city, zones, &zc, siteLoot, siteLootCount, curbs, curbCount);

    bool connected = ConnectivityOk(map);
    if (!connected && retry < 4) {
        // A gate can still be narrowed by a badly placed prop. Retrying the
        // complete city from a derived seed is deterministic, so host and
        // clients continue to build byte-identical maps.
        uint32_t next = map->seed ^ (0x9E3779B9u * (uint32_t)(retry + 1));
        World_Random_Attempt(map, mode, next, retry + 1);
        map->seed = seed ? seed : 1u;
        return;
    }
    if (!connected) {
        TraceLog(LOG_WARNING, "WORLD: city connectivity failed after retries; using open cover fallback");
        SafeOpenFallback(map, zones, zc);
    }

    AddRandomDecor(map);
    TraceLog(LOG_INFO,
             "WORLD: city mode=%d seed=%u limit=%.0f sites=%d boxes=%d pickups=%d barrels=%d lamps=%d",
             (int)mode, map->seed, map->limit, sites, map->boxCount,
             map->pickupCount, map->barrelCount, map->streetLampCount);
}

// ---------------------------------------------------------------------------
// Layout checksum, bot navigation grid and A* fallback
// ---------------------------------------------------------------------------

static uint64_t HashBytes(uint64_t h, const void* data, size_t n) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t HashU32(uint64_t h, uint32_t v) {
    return HashBytes(h, &v, sizeof(v));
}

static uint64_t HashF32(uint64_t h, float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return HashU32(h, bits);
}

static uint64_t HashVec3(uint64_t h, Vector3 v) {
    h = HashF32(h, v.x);
    h = HashF32(h, v.y);
    return HashF32(h, v.z);
}

uint64_t World_LayoutHash(const WorldMap* map) {
    if (!map) return 0;
    uint64_t h = 14695981039346656037ULL;
    h = HashU32(h, (uint32_t)map->mode);
    h = HashU32(h, map->seed);
    h = HashF32(h, map->limit);
    h = HashU32(h, (uint32_t)map->boxCount);
    for (int i = 0; i < map->boxCount; i++) {
        h = HashVec3(h, map->boxes[i].center);
        h = HashVec3(h, map->boxes[i].halfExtents);
    }
    for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++)
        h = HashVec3(h, map->playerSpawns[i]);
    h = HashU32(h, (uint32_t)map->enemySpawnCount);
    for (int i = 0; i < map->enemySpawnCount; i++)
        h = HashVec3(h, map->enemySpawns[i]);
    h = HashU32(h, (uint32_t)map->barrelCount);
    for (int i = 0; i < map->barrelCount; i++)
        h = HashVec3(h, map->barrelSpots[i]);
    h = HashU32(h, (uint32_t)map->pickupCount);
    for (int i = 0; i < map->pickupCount; i++) {
        h = HashVec3(h, map->pickupSpots[i]);
        h = HashU32(h, (uint32_t)map->pickupTypes[i]);
    }
    h = HashU32(h, (uint32_t)map->streetLampCount);
    for (int i = 0; i < map->streetLampCount; i++)
        h = HashVec3(h, map->streetLamps[i]);
    h = HashU32(h, (uint32_t)map->accentLampCount);
    for (int i = 0; i < map->accentLampCount; i++)
        h = HashVec3(h, map->accentLamps[i]);
    h = HashU32(h, (uint32_t)map->siteCount);
    for (int i = 0; i < map->siteCount; i++)
        h = HashU32(h, (uint32_t)map->siteKinds[i]);
    for (int i = 0; i < 4; i++)
        h = HashF32(h, map->cityCoords[i]);
    h = HashF32(h, map->cityBlockHalf);
    h = HashF32(h, map->cityRoadHalf);
    h = HashF32(h, map->cityEdgeRoad);
    return h;
}

// Rasterize the collision geometry into the coarse grid. This is O(box area),
// not O(cells * boxes), which keeps map generation cheap even on 256x256.
static void World_BuildNav(WorldMap* map) {
    memset(map->navBlocked, 0, sizeof(map->navBlocked));
    map->navCell = (map->limit * 2.0f) / (float)WORLD_NAV_DIM;
    map->navReady = false;
    if (map->limit <= 0.01f || map->navCell <= 0.01f) return;

    const float origin = -map->limit;
    const float radius = 0.38f;
    for (int i = 0; i < map->boxCount; i++) {
        const Box* b = &map->boxes[i];
        float top = b->center.y + b->halfExtents.y;
        float bottom = b->center.y - b->halfExtents.y;
        if (top <= 0.0f || bottom >= 1.80f) continue; // overhead canopy/roof
        float minX = b->center.x - b->halfExtents.x - radius;
        float maxX = b->center.x + b->halfExtents.x + radius;
        float minZ = b->center.z - b->halfExtents.z - radius;
        float maxZ = b->center.z + b->halfExtents.z + radius;
        int x0 = (int)ceilf((minX - origin) / map->navCell - 0.5f);
        int x1 = (int)floorf((maxX - origin) / map->navCell - 0.5f);
        int z0 = (int)ceilf((minZ - origin) / map->navCell - 0.5f);
        int z1 = (int)floorf((maxZ - origin) / map->navCell - 0.5f);
        if (x0 < 0) x0 = 0;
        if (z0 < 0) z0 = 0;
        if (x1 >= WORLD_NAV_DIM) x1 = WORLD_NAV_DIM - 1;
        if (z1 >= WORLD_NAV_DIM) z1 = WORLD_NAV_DIM - 1;
        for (int z = z0; z <= z1; z++) {
            float cz = origin + ((float)z + 0.5f) * map->navCell;
            for (int x = x0; x <= x1; x++) {
                float cx = origin + ((float)x + 0.5f) * map->navCell;
                if (cx >= minX && cx <= maxX && cz >= minZ && cz <= maxZ)
                    map->navBlocked[z * WORLD_NAV_DIM + x] = 1;
            }
        }
    }
    map->navReady = true;
}

static bool NavCellBlocked(const WorldMap* map, int x, int z) {
    if (!map || !map->navReady) return true;
    if (x < 0 || z < 0 || x >= WORLD_NAV_DIM || z >= WORLD_NAV_DIM) return true;
    return map->navBlocked[z * WORLD_NAV_DIM + x] != 0;
}

static void NavWorldToCell(const WorldMap* map, Vector3 p, int* outX, int* outZ) {
    float cell = map->navCell;
    int x = (int)floorf((p.x + map->limit) / cell);
    int z = (int)floorf((p.z + map->limit) / cell);
    if (x < 0) x = 0;
    if (z < 0) z = 0;
    if (x >= WORLD_NAV_DIM) x = WORLD_NAV_DIM - 1;
    if (z >= WORLD_NAV_DIM) z = WORLD_NAV_DIM - 1;
    *outX = x;
    *outZ = z;
}

static bool NavNearestOpen(const WorldMap* map, int cx, int cz, int* outX, int* outZ) {
    if (!NavCellBlocked(map, cx, cz)) {
        *outX = cx;
        *outZ = cz;
        return true;
    }
    for (int r = 1; r <= 8; r++) {
        int best = -1;
        int bestD = 0;
        for (int z = cz - r; z <= cz + r; z++) {
            for (int x = cx - r; x <= cx + r; x++) {
                if (x < 0 || z < 0 || x >= WORLD_NAV_DIM || z >= WORLD_NAV_DIM) continue;
                if (x > cx - r && x < cx + r && z > cz - r && z < cz + r) continue;
                if (NavCellBlocked(map, x, z)) continue;
                int d = (x - cx) * (x - cx) + (z - cz) * (z - cz);
                if (best < 0 || d < bestD) {
                    best = z * WORLD_NAV_DIM + x;
                    bestD = d;
                }
            }
        }
        if (best >= 0) {
            *outX = best % WORLD_NAV_DIM;
            *outZ = best / WORLD_NAV_DIM;
            return true;
        }
    }
    return false;
}

static float NavG[WORLD_NAV_NODES];
static int NavCame[WORLD_NAV_NODES];
static unsigned char NavClosed[WORLD_NAV_NODES];
static int NavHeap[WORLD_NAV_NODES * 8];
static int NavHeapCount;
static int NavGoalNode;

static float NavHeuristic(int a, int b) {
    int ax = a % WORLD_NAV_DIM, az = a / WORLD_NAV_DIM;
    int bx = b % WORLD_NAV_DIM, bz = b / WORLD_NAV_DIM;
    int dx = ax > bx ? ax - bx : bx - ax;
    int dz = az > bz ? az - bz : bz - az;
    int mn = dx < dz ? dx : dz;
    int mx = dx > dz ? dx : dz;
    return (float)(mx - mn) + 1.41421356f * (float)mn;
}

static void NavHeapPush(int id) {
    int i = NavHeapCount++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        int a = NavHeap[parent];
        float fa = NavG[a] + NavHeuristic(a, NavGoalNode);
        float fb = NavG[id] + NavHeuristic(id, NavGoalNode);
        if (fa < fb || (fa == fb && a < id)) break;
        NavHeap[i] = a;
        i = parent;
    }
    NavHeap[i] = id;
}

static int NavHeapPop(void) {
    int top = NavHeap[0];
    int last = NavHeap[--NavHeapCount];
    int i = 0;
    while (true) {
        int child = i * 2 + 1;
        if (child >= NavHeapCount) break;
        if (child + 1 < NavHeapCount) {
            int a = NavHeap[child], b = NavHeap[child + 1];
            float fa = NavG[a] + NavHeuristic(a, NavGoalNode);
            float fb = NavG[b] + NavHeuristic(b, NavGoalNode);
            if (fb < fa || (fb == fa && b < a)) child++;
        }
        int a = NavHeap[child];
        float fa = NavG[a] + NavHeuristic(a, NavGoalNode);
        float fl = NavG[last] + NavHeuristic(last, NavGoalNode);
        if (fl < fa || (fl == fa && last < a)) break;
        NavHeap[i] = a;
        i = child;
    }
    if (NavHeapCount > 0) NavHeap[i] = last;
    return top;
}

static bool NavSegmentClear(const WorldMap* map, Vector3 a, Vector3 b) {
    float dx = b.x - a.x, dz = b.z - a.z;
    float dist = sqrtf(dx * dx + dz * dz);
    int steps = (int)(dist / (map->navCell * 0.35f)) + 1;
    for (int i = 1; i < steps; i++) {
        float t = (float)i / (float)steps;
        int cx, cz;
        NavWorldToCell(map, (Vector3){ a.x + dx * t, 0.9f, a.z + dz * t }, &cx, &cz);
        if (NavCellBlocked(map, cx, cz)) return false;
    }
    return true;
}

int World_NavPath(const WorldMap* map, Vector3 from, Vector3 to,
                  Vector3* outPath, int maxPoints) {
    if (!map || !map->navReady || !outPath || maxPoints <= 0) return 0;
    int sx, sz, gx, gz;
    NavWorldToCell(map, from, &sx, &sz);
    NavWorldToCell(map, to, &gx, &gz);
    if (!NavNearestOpen(map, sx, sz, &sx, &sz)) return 0;
    if (!NavNearestOpen(map, gx, gz, &gx, &gz)) return 0;
    int start = sz * WORLD_NAV_DIM + sx;
    int goal = gz * WORLD_NAV_DIM + gx;
    if (start == goal) return 0;

    for (int i = 0; i < WORLD_NAV_NODES; i++) {
        NavG[i] = 1e30f;
        NavCame[i] = -1;
        NavClosed[i] = 0;
    }
    NavHeapCount = 0;
    NavGoalNode = goal;
    NavG[start] = 0.0f;
    NavHeapPush(start);

    static const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    static const int dz[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    bool found = false;
    while (NavHeapCount > 0) {
        int cur = NavHeapPop();
        if (NavClosed[cur]) continue;
        NavClosed[cur] = 1;
        if (cur == goal) { found = true; break; }
        int cx = cur % WORLD_NAV_DIM, cz = cur / WORLD_NAV_DIM;
        for (int d = 0; d < 8; d++) {
            int nx = cx + dx[d], nz = cz + dz[d];
            if (NavCellBlocked(map, nx, nz)) continue;
            if (dx[d] && dz[d] &&
                (NavCellBlocked(map, cx + dx[d], cz) ||
                 NavCellBlocked(map, cx, cz + dz[d]))) continue;
            int ni = nz * WORLD_NAV_DIM + nx;
            if (NavClosed[ni]) continue;
            float step = (dx[d] && dz[d]) ? 1.41421356f : 1.0f;
            float ng = NavG[cur] + step;
            if (ng >= NavG[ni]) continue;
            NavG[ni] = ng;
            NavCame[ni] = cur;
            NavHeapPush(ni);
        }
    }
    if (!found) return 0;

    static int nodes[WORLD_NAV_NODES];
    int nodeCount = 0;
    for (int n = goal; n >= 0 && nodeCount < WORLD_NAV_NODES; n = NavCame[n])
        nodes[nodeCount++] = n;
    // nodes are goal..start; reverse to start..goal for smoothing.
    static Vector3 raw[WORLD_NAV_NODES];
    int rawCount = 0;
    for (int i = nodeCount - 1; i >= 0; i--) {
        int n = nodes[i];
        int x = n % WORLD_NAV_DIM, z = n / WORLD_NAV_DIM;
        raw[rawCount++] = (Vector3){
            -map->limit + ((float)x + 0.5f) * map->navCell, 0.9f,
            -map->limit + ((float)z + 0.5f) * map->navCell,
        };
    }

    int outCount = 0;
    int anchor = 0;
    while (anchor < rawCount - 1 && outCount < maxPoints) {
        int next = rawCount - 1;
        while (next > anchor + 1 && !NavSegmentClear(map, raw[anchor], raw[next]))
            next--;
        if (next <= anchor) break;
        outPath[outCount++] = raw[next];
        anchor = next;
    }
    if (outCount > 0) {
        Vector3 last = outPath[outCount - 1];
        if (NavSegmentClear(map, last, to) &&
            !World_MoveCollides((Vector3){ to.x, 0.9f, to.z },
                                (Vector3){ 0.35f, 0.9f, 0.35f },
                                map->boxes, map->boxCount))
            outPath[outCount - 1] = (Vector3){ to.x, 0.9f, to.z };
    }
    return outCount;
}

static void World_Random(WorldMap* map, WorldMode mode, uint32_t seed) {
    World_Random_Attempt(map, mode, seed, 0);
    map->seed = seed ? seed : 1u;
}

void World_Generate(WorldMap* map, WorldMode mode, uint32_t seed) {
    if (!map) return;
    if (mode == WORLD_CLASSIC) {
        memset(map, 0, sizeof(*map));
        World_Classic(map);
        World_BuildNav(map);
        TraceLog(LOG_INFO, "WORLD: classic layout (%d boxes, %d pickups) hash=%016llx",
                 map->boxCount, map->pickupCount,
                 (unsigned long long)World_LayoutHash(map));
        return;
    }
    if (mode != WORLD_RANDOM && mode != WORLD_RANDOM_256) mode = WORLD_RANDOM;
    World_Random(map, mode, seed);
    World_BuildNav(map);
    TraceLog(LOG_INFO, "WORLD: layout hash=%016llx",
             (unsigned long long)World_LayoutHash(map));
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

void World_DrawSky(float time, Vector3 camPos, float limit) {
    DrawSkyDome(camPos);
    float L = limit;
    // Opaque sun + mountains (no alpha sorting bugs). Everything sits outside
    // the playable envelope so a 256x256 map never has scenery growing through
    // its centre.
    DrawSphereEx((Vector3){ L * 1.75f, 70.0f, -L * 1.75f }, 9.0f, 16, 12,
                 (Color){ 255, 244, 210, 255 });
    Color mcol = FogShade((Color){ 100, 116, 142, 255 }, 110.0f);
    Color mhi = FogShade((Color){ 160, 175, 195, 255 }, 110.0f);
    DrawCube((Vector3){ -L - 22, 6, -L - 10 }, 30, 14, 8, mcol);
    DrawCube((Vector3){ -L - 22, 9.5f, -L - 10 }, 30, 1.2f, 8.2f, mhi);
    DrawCube((Vector3){ L + 18, 5, -L - 18 }, 36, 12, 10, mcol);
    DrawCube((Vector3){ L + 18, 8.2f, -L - 18 }, 36, 1.0f, 10.2f, mhi);
    DrawCube((Vector3){ -L - 18, 5, L + 18 }, 34, 11, 9, mcol);
    DrawCube((Vector3){ L + 22, 6, L + 14 }, 28, 13, 8, mcol);

    // Opaque horizon band (replaces transparent haze cubes)
    float band = L + 20.0f;
    float bw = 2.0f * band + 50.0f;
    DrawCube((Vector3){ 0, 2.2f, -band }, bw, 4.5f, 1.2f, (Color){ 195, 215, 232, 255 });
    DrawCube((Vector3){ 0, 2.2f,  band }, bw, 4.5f, 1.2f, (Color){ 195, 215, 232, 255 });
    DrawCube((Vector3){ -band, 2.2f, 0 }, 1.2f, 4.5f, bw, (Color){ 195, 215, 232, 255 });
    DrawCube((Vector3){  band, 2.2f, 0 }, 1.2f, 4.5f, bw, (Color){ 195, 215, 232, 255 });

    // Opaque clouds (fixes transparent sphere sorting)
    float span = 2.0f * (L + 25.0f);
    for (int i = 0; i < 8; i++) {
        float sp = 0.5f + 0.16f * (float)i;
        float x = fmodf((float)i * 31.0f + time * sp * 2.2f, span) - span * 0.5f;
        float z = -L * 0.7f + (float)(i % 4) * (L * 0.45f);
        float y = 25.0f + (float)(i % 4) * 3.2f;
        Color c = { 252, 253, 255, 255 };
        Color sh = { 218, 226, 236, 255 };
        DrawSphereEx((Vector3){ x, y - 0.55f, z }, 3.3f, 12, 8, sh);
        DrawSphereEx((Vector3){ x, y, z }, 3.2f, 12, 8, c);
        DrawSphereEx((Vector3){ x + 3.1f, y - 0.4f, z }, 2.3f, 10, 8, c);
        DrawSphereEx((Vector3){ x - 3.1f, y - 0.5f, z }, 2.0f, 10, 8, c);
    }
    for (int i = 0; i < 5; i++) {
        float bx = fmodf((float)i * 53.0f + time * (3.0f + i), span) - span * 0.5f;
        float by = 20.0f + sinf(time * 0.7f + i * 1.7f) * 1.5f;
        float bz = -L * 0.55f + (i % 3) * (L * 0.45f);
        float flap = sinf(time * 9.0f + i * 2.0f) * 0.35f;
        Vector3 p = { bx, by, bz };
        DrawLine3D((Vector3){ p.x - 0.6f, p.y + flap, p.z }, p, BLACK);
        DrawLine3D(p, (Vector3){ p.x + 0.6f, p.y + flap, p.z }, BLACK);
    }
}

// Layered heights: ground 0 -> patches 0.01 -> roads 0.06 top -> dashes 0.08
// -> shadows 0.10 -> decals 0.12. No coplanar planes.
static void DrawClassicProps(const WorldMap* map, float time, Vector3 camPos) {
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

    for (int i = 0; i < map->streetLampCount; i++) {
        Vector3 base = { map->streetLamps[i].x, 0.0f, map->streetLamps[i].z };
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

static float Hash01(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return (float)(x >> 8) * (1.0f / 16777216.0f);
}

// Pavement hierarchy for random maps: block pads and curb rings first, then
// asphalt roads, then lane paint. All geometry is opaque and sits a few
// millimetres above the previous layer, so there is no coplanar flicker.
static void DrawCityGround(const WorldMap* map) {
    if (map->cityCols <= 0) return;
    const float L = map->limit;
    const float bh = map->cityBlockHalf;
    const float rw = map->cityRoadHalf * 2.0f;
    Color sidewalk = (Color){ 151, 147, 137, 255 };
    Color sidewalk2 = (Color){ 160, 155, 144, 255 };
    Color curb = (Color){ 181, 177, 166, 255 };
    Color asphalt = (Color){ 57, 59, 64, 255 };
    Color dash = (Color){ 224, 196, 92, 255 };
    int grid = map->cityCols;
    for (int gz = 0; gz < grid; gz++) {
        for (int gx = 0; gx < grid; gx++) {
            float cx = map->cityCoords[gx];
            float cz = map->cityCoords[gz];
            int kind = map->siteKinds[gz * grid + gx];
            Color pad = (kind == 5 || kind == 8) ? (Color){ 89, 119, 81, 255 }
                       : (kind == 6) ? (Color){ 153, 140, 118, 255 }
                       : (kind == 7) ? (Color){ 139, 126, 101, 255 }
                       : ((gx + gz) & 1) ? sidewalk : sidewalk2;
            DrawCube((Vector3){ cx, 0.002f, cz }, bh * 2.0f, 0.020f, bh * 2.0f, pad);
            if (kind == 5 || kind == 8) {
                Color path = (Color){ 164, 159, 147, 255 };
                DrawCube((Vector3){ cx, 0.014f, cz }, bh * 1.55f, 0.012f, 2.3f, path);
                DrawCube((Vector3){ cx, 0.014f, cz }, 2.3f, 0.012f, bh * 1.55f, path);
            }
            DrawCube((Vector3){ cx, 0.015f, cz - bh }, bh * 2.0f, 0.030f, 0.18f, curb);
            DrawCube((Vector3){ cx, 0.015f, cz + bh }, bh * 2.0f, 0.030f, 0.18f, curb);
            DrawCube((Vector3){ cx - bh, 0.015f, cz }, 0.18f, 0.030f, bh * 2.0f, curb);
            DrawCube((Vector3){ cx + bh, 0.015f, cz }, 0.18f, 0.030f, bh * 2.0f, curb);
        }
    }
    float roads[4];
    int rn = map->cityCols - 1;
    for (int i = 0; i < rn; i++) {
        roads[i] = (map->cityCoords[i] + map->cityCoords[i + 1]) * 0.5f;
    }
    for (int i = 0; i < rn; i++) {
        DrawCube((Vector3){ roads[i], 0.010f, 0 }, rw, 0.030f, L * 2.0f, asphalt);
        DrawCube((Vector3){ 0, 0.010f, roads[i] }, L * 2.0f, 0.030f, rw, asphalt);
    }
    float edge = map->cityEdgeRoad;
    DrawCube((Vector3){  edge, 0.010f, 0 }, 7.0f, 0.030f, L * 2.0f, asphalt);
    DrawCube((Vector3){ -edge, 0.010f, 0 }, 7.0f, 0.030f, L * 2.0f, asphalt);
    DrawCube((Vector3){ 0, 0.010f,  edge }, L * 2.0f, 0.030f, 7.0f, asphalt);
    DrawCube((Vector3){ 0, 0.010f, -edge }, L * 2.0f, 0.030f, 7.0f, asphalt);
    for (int i = 0; i < rn; i++) {
        for (float z = -L + 12.0f; z <= L - 12.0f; z += 12.0f) {
            bool near = false;
            for (int k = 0; k < rn; k++) {
                if (fabsf(z - roads[k]) < map->cityRoadHalf + 2.0f) { near = true; break; }
            }
            if (!near) DrawCube((Vector3){ roads[i], 0.040f, z }, 0.16f, 0.012f, 3.0f, dash);
        }
        for (float x = -L + 12.0f; x <= L - 12.0f; x += 12.0f) {
            bool near = false;
            for (int k = 0; k < rn; k++) {
                if (fabsf(x - roads[k]) < map->cityRoadHalf + 2.0f) { near = true; break; }
            }
            if (!near) DrawCube((Vector3){ x, 0.040f, roads[i] }, 3.0f, 0.012f, 0.16f, dash);
        }
    }
}

static void DrawRandomGroundPatches(const WorldMap* map) {
    float range = map->limit * 0.78f;
    for (int i = 0; i < 6; i++) {
        float x = (Hash01(map->seed + (uint32_t)i * 977u) * 2.0f - 1.0f) * range;
        float z = (Hash01(map->seed + (uint32_t)i * 1597u + 31u) * 2.0f - 1.0f) * range;
        float w = 18.0f + Hash01(map->seed + (uint32_t)i * 71u) * 38.0f;
        float h = 14.0f + Hash01(map->seed + (uint32_t)i * 131u) * 30.0f;
        Color c = (i & 1) ? (Color){ 168, 148, 112, 255 } : (Color){ 172, 160, 118, 255 };
        DrawPlane((Vector3){ x, -0.012f - (float)i * 0.0002f, z }, (Vector2){ w, h }, c);
    }
}

static void DrawRandomProps(const WorldMap* map, float time, Vector3 camPos) {
    (void)camPos;
    const float L = map->limit;

    // Street lamps are map-owned, so a reshuffled world never leaves a light
    // floating where the old post used to stand.
    for (int i = 0; i < map->streetLampCount; i++) {
        float x = map->streetLamps[i].x, z = map->streetLamps[i].z;
        DrawCylinderEx((Vector3){ x, 0, z }, (Vector3){ x, 0.15f, z }, 0.30f, 0.34f, 8,
                       (Color){ 50, 50, 55, 255 });
        DrawCylinderEx((Vector3){ x, 0.12f, z }, (Vector3){ x, 4.6f, z }, 0.09f, 0.09f, 8,
                       (Color){ 45, 46, 50, 255 });
        DrawCube((Vector3){ x, 4.7f, z }, 0.72f, 0.18f, 0.36f, (Color){ 40, 41, 45, 255 });
        DrawSphereEx((Vector3){ x + 0.2f, 4.55f, z }, 0.14f, 10, 8,
                     (Color){ 255, 250, 220, 255 });
    }

    // Pebbles and rocks are below knee height; they are scenery, not cover.
    for (int i = 0; i < map->pebbleCount; i++) {
        Vector3 p = map->pebbles[i];
        float s = map->pebbleSize[i];
        DrawCubeV(p, (Vector3){ s, 0.07f, s }, (Color){ 122, 107, 87, 255 });
    }
    for (int i = 0; i < map->rockCount; i++) {
        Vector3 r = map->rocks[i];
        float s = map->rockSize[i];
        DrawSphereEx((Vector3){ r.x, s * 0.35f, r.z }, s, 9, 6, (Color){ 126, 123, 116, 255 });
        DrawSphereEx((Vector3){ r.x + s * 0.4f, s * 0.18f, r.z + s * 0.24f }, s * 0.45f, 8, 6,
                     (Color){ 146, 143, 136, 255 });
        DrawSphereEx((Vector3){ r.x - s * 0.25f, s * 0.72f, r.z }, s * 0.42f, 8, 6,
                     (Color){ 155, 152, 145, 255 });
    }
    for (int i = 0; i < map->grassCount; i++) {
        Vector3 g = map->grass[i];
        float s = map->grassSize[i];
        float sway = sinf(time * 1.7f + (float)i * 0.7f) * 0.015f;
        Color gc = (i & 1) ? (Color){ 88, 142, 72, 255 } : (Color){ 112, 157, 82, 255 };
        DrawCubeV((Vector3){ g.x + sway, g.y, g.z }, (Vector3){ s, 0.24f, s }, gc);
        DrawCubeV((Vector3){ g.x + sway * 2.0f, g.y + 0.14f, g.z },
                  (Vector3){ s * 0.55f, 0.10f, s * 0.55f }, (Color){ 142, 182, 102, 255 });
    }
    (void)L;
}

void World_DrawLit(const WorldMap* map, float time, Vector3 camPos) {
    if (!map) return;
    // Opaque ground first. NOTE: DrawGrid draws at y=0, so ground must sit
    // BELOW it (y=-0.02) or the two coplanar surfaces z-fight and flicker
    // whenever the camera moves.
    float gs = map->groundSize;
    DrawPlane((Vector3){ 0, -0.02f, 0 }, (Vector2){ gs, gs },
              (Color){ 178, 160, 122, 255 });
    if (map->mode == WORLD_CLASSIC) {
        // Large soft variation (few big quads = no moire shimmer)
        DrawPlane((Vector3){ -20, -0.012f, -10 }, (Vector2){ 34, 24 }, (Color){ 168, 148, 112, 255 });
        DrawPlane((Vector3){ 22, -0.012f, 18 }, (Vector2){ 28, 22 }, (Color){ 172, 160, 118, 255 });
        DrawPlane((Vector3){ 5, -0.011f, -30 }, (Vector2){ 42, 17 }, (Color){ 165, 148, 112, 255 });
        DrawClassicProps(map, time, camPos);
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
    } else {
        DrawRandomGroundPatches(map);
        DrawCityGround(map);
        DrawRandomProps(map, time, camPos);
    }
    for (int i = 0; i < map->boxCount; i++) DrawBoxEnhanced(map->boxes[i], camPos);
}

void World_DrawBoxes(const WorldMap* map) {
    if (!map) return;
    for (int i = 0; i < map->boxCount; i++) DrawBoxEnhanced(map->boxes[i], (Vector3){ 0 });
}

void World_DrawCasters(const WorldMap* map) {
    if (!map) return;
    for (int i = 0; i < map->boxCount; i++) {
        const Box* b = &map->boxes[i];
        DrawCube(b->center, b->halfExtents.x * 2.0f, b->halfExtents.y * 2.0f,
                 b->halfExtents.z * 2.0f, b->color);
    }
}

// Grid lines carry no reliable normals: keep them on the default shader.
void World_DrawGrid(const WorldMap* map) {
    if (!map) return;
    // Random maps have painted roads and sidewalk pads; the 2 m debug grid
    // would cut straight through the asphalt and make the city read as a
    // testing range.
    if (map->mode != WORLD_CLASSIC) return;
    int slices = (int)(map->groundSize / 2.0f);
    if (slices < 24) slices = 24;
    if (slices > 180) slices = 180;
    DrawGrid(slices, 2.0f);
}
