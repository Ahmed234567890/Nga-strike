#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// In-repo regression tests for deterministic world generation, layout hashing
// and the coarse bot navigation fallback. Run with: ninja check

static int failures;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);               \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static int CellIndex(const WorldMap* m, float x, float z, int* outX, int* outZ) {
    if (!m->navReady || m->navCell <= 0.0f) return -1;
    int cx = (int)floorf((x + m->limit) / m->navCell);
    int cz = (int)floorf((z + m->limit) / m->navCell);
    if (cx < 0) cx = 0;
    if (cz < 0) cz = 0;
    if (cx >= WORLD_NAV_DIM) cx = WORLD_NAV_DIM - 1;
    if (cz >= WORLD_NAV_DIM) cz = WORLD_NAV_DIM - 1;
    if (outX) *outX = cx;
    if (outZ) *outZ = cz;
    return cz * WORLD_NAV_DIM + cx;
}

static int CellOpen(const WorldMap* m, int cx, int cz) {
    if (cx < 0 || cz < 0 || cx >= WORLD_NAV_DIM || cz >= WORLD_NAV_DIM)
        return 0;
    return m->navBlocked[cz * WORLD_NAV_DIM + cx] == 0;
}

static int NearestOpenCell(const WorldMap* m, int cx, int cz, int* ox, int* oz) {
    for (int r = 0; r <= 3; r++) {
        int best = -1;
        int bestD = 0;
        for (int z = cz - r; z <= cz + r; z++) {
            for (int x = cx - r; x <= cx + r; x++) {
                if (r > 0 && x > cx - r && x < cx + r &&
                    z > cz - r && z < cz + r)
                    continue;
                if (!CellOpen(m, x, z)) continue;
                int d = (x - cx) * (x - cx) + (z - cz) * (z - cz);
                if (best < 0 || d < bestD) {
                    best = z * WORLD_NAV_DIM + x;
                    bestD = d;
                }
            }
        }
        if (best >= 0) {
            *ox = best % WORLD_NAV_DIM;
            *oz = best / WORLD_NAV_DIM;
            return 1;
        }
    }
    return 0;
}

// BFS over the coarse grid. The player must be able to reach every enemy
// spawn, barrel, pickup and lamp, otherwise bots and players get cut off from
// part of the arena even though map generation succeeded.
static int UnreachablePoints(const WorldMap* m) {
    int sx, sz;
    if (!NearestOpenCell(m,
                         (int)((m->playerSpawns[0].x + m->limit) / m->navCell),
                         (int)((m->playerSpawns[0].z + m->limit) / m->navCell),
                         &sx, &sz))
        return 1;

    static unsigned char seen[WORLD_NAV_NODES];
    static int queue[WORLD_NAV_NODES];
    memset(seen, 0, sizeof(seen));
    int head = 0, tail = 0;
    int start = sz * WORLD_NAV_DIM + sx;
    seen[start] = 1;
    queue[tail++] = start;
    static const int dx[4] = { 1, -1, 0, 0 };
    static const int dz[4] = { 0, 0, 1, -1 };
    while (head < tail) {
        int id = queue[head++];
        int cx = id % WORLD_NAV_DIM;
        int cz = id / WORLD_NAV_DIM;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int nz = cz + dz[d];
            if (!CellOpen(m, nx, nz)) continue;
            int ni = nz * WORLD_NAV_DIM + nx;
            if (seen[ni]) continue;
            seen[ni] = 1;
            queue[tail++] = ni;
        }
    }

    int missing = 0;
    Vector3 points[4 + MAX_ENEMIES + MAX_BARRELS + MAX_PICKUPS +
                   WORLD_MAX_STREET_LAMPS];
    int count = 0;
    for (int i = 0; i < WORLD_MAX_PLAYER_SPAWNS; i++)
        points[count++] = m->playerSpawns[i];
    for (int i = 0; i < m->enemySpawnCount; i++)
        points[count++] = m->enemySpawns[i];
    for (int i = 0; i < m->barrelCount; i++)
        points[count++] = m->barrelSpots[i];
    for (int i = 0; i < m->pickupCount; i++)
        points[count++] = m->pickupSpots[i];
    for (int i = 0; i < m->streetLampCount; i++)
        points[count++] = m->streetLamps[i];
    for (int i = 0; i < count; i++) {
        int cx, cz, ox, oz;
        CellIndex(m, points[i].x, points[i].z, &cx, &cz);
        if (!NearestOpenCell(m, cx, cz, &ox, &oz) ||
            !seen[oz * WORLD_NAV_DIM + ox])
            missing++;
    }
    return missing;
}

static void CheckMapContent(const WorldMap* m, WorldMode mode, uint32_t seed) {
    int large = (mode == WORLD_RANDOM_256);
    int expectedCols = large ? 4 : 3;
    float expectedLimit = large ? 128.0f : 48.0f;
    int expectedPickups = large ? 20 : 8;
    CHECK(m->mode == mode, "mode %d seed %u: got mode %d", (int)mode, seed,
          (int)m->mode);
    CHECK(m->seed == seed, "mode %d seed %u: stored seed %u", (int)mode, seed,
          m->seed);
    CHECK(fabsf(m->limit - expectedLimit) < 0.01f,
          "mode %d seed %u: limit %.2f", (int)mode, seed, m->limit);
    CHECK(m->boxCount >= 60 && m->boxCount <= WORLD_MAX_BOXES,
          "mode %d seed %u: boxCount %d", (int)mode, seed, m->boxCount);
    CHECK(m->pickupCount == expectedPickups,
          "mode %d seed %u: pickupCount %d (expected %d)", (int)mode, seed,
          m->pickupCount, expectedPickups);
    CHECK(m->barrelCount == MAX_BARRELS,
          "mode %d seed %u: barrelCount %d", (int)mode, seed, m->barrelCount);
    CHECK(m->enemySpawnCount == MAX_ENEMIES,
          "mode %d seed %u: enemySpawnCount %d", (int)mode, seed,
          m->enemySpawnCount);
    CHECK(m->streetLampCount == WORLD_MAX_STREET_LAMPS,
          "mode %d seed %u: streetLampCount %d", (int)mode, seed,
          m->streetLampCount);
    CHECK(m->cityCols == expectedCols,
          "mode %d seed %u: cityCols %d (expected %d)", (int)mode, seed,
          m->cityCols, expectedCols);
    CHECK(m->siteCount == expectedCols * expectedCols,
          "mode %d seed %u: siteCount %d", (int)mode, seed, m->siteCount);
    CHECK(m->siteCount <= WORLD_MAX_SITES, "mode %d seed %u: too many sites",
          (int)mode, seed);
    CHECK(m->navReady && m->navCell > 0.0f,
          "mode %d seed %u: nav grid not built", (int)mode, seed);
    CHECK(fabsf(m->navCell - (2.0f * m->limit) / WORLD_NAV_DIM) < 0.0001f,
          "mode %d seed %u: navCell %.4f", (int)mode, seed, m->navCell);

    int blocked = 0;
    for (int i = 0; i < WORLD_NAV_NODES; i++) blocked += m->navBlocked[i] != 0;
    CHECK(blocked > WORLD_NAV_NODES / 50,
          "mode %d seed %u: nav grid almost empty (%d blocked)", (int)mode,
          seed, blocked);
    CHECK(blocked < WORLD_NAV_NODES * 3 / 4,
          "mode %d seed %u: nav grid almost solid (%d blocked)", (int)mode,
          seed, blocked);

    for (int i = 0; i < m->boxCount; i++) {
        const Box* b = &m->boxes[i];
        float loX = b->center.x - b->halfExtents.x;
        float hiX = b->center.x + b->halfExtents.x;
        float loZ = b->center.z - b->halfExtents.z;
        float hiZ = b->center.z + b->halfExtents.z;
        CHECK(loX >= -m->limit - 0.75f && hiX <= m->limit + 0.75f &&
              loZ >= -m->limit - 0.75f && hiZ <= m->limit + 0.75f,
              "mode %d seed %u: box %d outside bounds", (int)mode, seed, i);
    }
    for (int i = 0; i < m->siteCount; i++) {
        unsigned char k = m->siteKinds[i];
        CHECK(k <= 8, "mode %d seed %u: site %d kind %u", (int)mode, seed, i,
              (unsigned)k);
    }
    for (int i = 0; i < m->pickupCount; i++) {
        CHECK(m->pickupTypes[i] == PICKUP_HEALTH ||
              m->pickupTypes[i] == PICKUP_AMMO,
              "mode %d seed %u: pickup %d type %d", (int)mode, seed, i,
              (int)m->pickupTypes[i]);
    }

    int missing = UnreachablePoints(m);
    CHECK(missing == 0, "mode %d seed %u: %d points unreachable by nav grid",
          (int)mode, seed, missing);
}

static void CheckNavPath(const WorldMap* m, WorldMode mode, uint32_t seed) {
    Vector3 from = m->playerSpawns[0];
    for (int t = 0; t < 2; t++) {
        Vector3 to = (t == 0) ? m->enemySpawns[0] : m->pickupSpots[0];
        Vector3 path[WORLD_NAV_MAX_POINTS];
        int n = World_NavPath(m, from, to, path, WORLD_NAV_MAX_POINTS);
        CHECK(n > 0, "mode %d seed %u: no nav path to target %d", (int)mode,
              seed, t);
        if (n <= 0) continue;
        CHECK(n <= WORLD_NAV_MAX_POINTS, "mode %d seed %u: nav path too long",
              (int)mode, seed);
        for (int i = 0; i < n; i++) {
            int cx, cz;
            CellIndex(m, path[i].x, path[i].z, &cx, &cz);
            CHECK(isfinite(path[i].x) && isfinite(path[i].z),
                  "mode %d seed %u: non-finite waypoint", (int)mode, seed);
            CHECK(fabsf(path[i].x) <= m->limit + 0.01f &&
                  fabsf(path[i].z) <= m->limit + 0.01f,
                  "mode %d seed %u: waypoint outside map", (int)mode, seed);
            CHECK(CellOpen(m, cx, cz),
                  "mode %d seed %u: waypoint %d in blocked cell", (int)mode,
                  seed, i);
        }
        float endDist = Vector3Distance(path[n - 1],
                                        (Vector3){ to.x, 0.9f, to.z });
        CHECK(endDist <= 6.0f,
              "mode %d seed %u: nav path ends %.1fm from target", (int)mode,
              seed, endDist);
    }
}

static void TestClassic(void) {
    WorldMap a, b;
    World_Generate(&a, WORLD_CLASSIC, 0);
    World_Generate(&b, WORLD_CLASSIC, 12345);
    uint64_t hash = World_LayoutHash(&a);
    CHECK(hash == 0xb15336a866fa6517ULL,
          "classic layout hash changed: %016llx", (unsigned long long)hash);
    CHECK(World_LayoutHash(&b) == hash,
          "classic layout must ignore the random seed");
    CHECK(a.boxCount == 92, "classic boxCount %d", a.boxCount);
    CHECK(a.pickupCount == 8, "classic pickupCount %d", a.pickupCount);
    CHECK(a.navReady && a.navCell > 0.0f, "classic nav grid not built");
    CHECK(a.seed == 0, "classic seed should stay 0");

    WorldMap nav = a;
    Vector3 path[WORLD_NAV_MAX_POINTS];
    int n = World_NavPath(&nav, nav.playerSpawns[0], (Vector3){ 20, 0.9f, -20 },
                          path, WORLD_NAV_MAX_POINTS);
    CHECK(n > 0, "classic nav path failed");

    // A layout hash must be sensitive to gameplay geometry, not just metadata.
    WorldMap edited = a;
    edited.boxes[0].center.x += 0.5f;
    CHECK(World_LayoutHash(&edited) != hash,
          "layout hash ignored a moved collision box");
    edited = a;
    edited.pickupSpots[0].x += 0.5f;
    CHECK(World_LayoutHash(&edited) != hash,
          "layout hash ignored a moved pickup");
    edited = a;
    edited.playerSpawns[0].z += 0.5f;
    CHECK(World_LayoutHash(&edited) != hash,
          "layout hash ignored a moved player spawn");
}

static void TestRandomSweep(void) {
    static WorldMap first, second;
    for (int i = 0; i < 120; i++) {
        WorldMode mode = (i & 1) ? WORLD_RANDOM : WORLD_RANDOM_256;
        uint32_t seed = 1u + (uint32_t)i * 2654435761u;
        World_Generate(&first, mode, seed);
        CheckMapContent(&first, mode, seed);
        uint64_t hash = World_LayoutHash(&first);

        World_Generate(&second, mode, seed);
        CHECK(World_LayoutHash(&second) == hash,
              "mode %d seed %u: hash differs on regeneration", (int)mode, seed);
        CHECK(memcmp(&first, &second, sizeof(first)) == 0,
              "mode %d seed %u: map bytes differ on regeneration", (int)mode,
              seed);

        if (i < 20) CheckNavPath(&first, mode, seed);
    }

    // Seed 0 is normalised to 1, documented as the minimum usable seed.
    World_Generate(&first, WORLD_RANDOM, 0);
    World_Generate(&second, WORLD_RANDOM, 1);
    CHECK(first.seed == 1 && World_LayoutHash(&first) == World_LayoutHash(&second),
          "seed 0 must behave like seed 1");

    // Different seeds must not collapse onto one layout.
    World_Generate(&first, WORLD_RANDOM, 1);
    World_Generate(&second, WORLD_RANDOM, 42);
    CHECK(World_LayoutHash(&first) != World_LayoutHash(&second),
          "different seeds produced the same hash");
}

static void TestFixedHashes(void) {
    struct Case {
        WorldMode mode;
        uint32_t seed;
        uint64_t hash;
    } cases[] = {
        { WORLD_RANDOM, 1, 0x251345f5a3e7cbb1ULL },
        { WORLD_RANDOM, 42, 0xf647d74a35fa8bf6ULL },
        { WORLD_RANDOM, 123456789u, 0xad942963ca25223fULL },
        { WORLD_RANDOM_256, 1, 0xd4068d707973f1cdULL },
        { WORLD_RANDOM_256, 42, 0xb4880f9746411137ULL },
        { WORLD_RANDOM_256, 123456789u, 0xa5b68cdf1bc01d2fULL },
    };
    static WorldMap m;
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        World_Generate(&m, cases[i].mode, cases[i].seed);
        uint64_t h = World_LayoutHash(&m);
        CHECK(h == cases[i].hash,
              "mode %d seed %u: expected hash %016llx, got %016llx",
              (int)cases[i].mode, cases[i].seed,
              (unsigned long long)cases[i].hash, (unsigned long long)h);
    }
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    TestClassic();
    TestFixedHashes();
    TestRandomSweep();
    if (failures) {
        fprintf(stderr, "test_world: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_world: classic, fixed hashes and 120 deterministic city seeds OK\n");
    return 0;
}
