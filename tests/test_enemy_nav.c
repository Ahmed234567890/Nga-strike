#include "enemy.h"
#include "world.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Regression test for the bot stuck detector and coarse A* fallback. A bot is
// placed in a dead-end corridor facing the target through the end wall. Local
// steering cannot slide out, so after 1.25 s it must log AI STUCK, request a
// World_NavPath around the corridor and follow it back out.

static int failures;
static int stuckLogs;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);               \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void CaptureLog(int level, const char* text, va_list args) {
    (void)level;
    char buf[512];
    vsnprintf(buf, sizeof(buf), text, args);
    if (strstr(buf, "AI STUCK")) {
        stuckLogs++;
        printf("captured: %s\n", buf);
    }
}

static void AddWall(WorldMap* m, Vector3 center, Vector3 half) {
    m->boxes[m->boxCount++] =
        (Box){ center, half, (Color){ 100, 100, 100, 255 } };
}

// 3x3 city-sized bounds are irrelevant here; only limit, boxes and the nav
// raster are used by Enemy_UpdateAll.
static void BuildNavGrid(WorldMap* m) {
    m->navCell = (2.0f * m->limit) / WORLD_NAV_DIM;
    m->navReady = true;
    Vector3 half = { 0.35f, 0.9f, 0.35f };
    for (int z = 0; z < WORLD_NAV_DIM; z++) {
        for (int x = 0; x < WORLD_NAV_DIM; x++) {
            Vector3 p = { -m->limit + ((float)x + 0.5f) * m->navCell, 0.9f,
                          -m->limit + ((float)z + 0.5f) * m->navCell };
            m->navBlocked[z * WORLD_NAV_DIM + x] =
                World_MoveCollides(p, half, m->boxes, m->boxCount);
        }
    }
}

static void SetupEnemy(Enemy* e, Vector3 position, Vector3 target) {
    Enemy_InitAll(e, 1, NULL, 0);
    e->position = position;
    e->spawnPoint = position;
    e->homePos = position;
    e->lastPos = position;
    e->alive = true;
    e->health = e->maxHealth = 100.0f;
    e->state = E_CHASE;
    e->alertTimer = 30.0f;
    e->lastSeenPos = target;
}

static void SetupTarget(Player* p, EnemyTarget* t, Vector3 position) {
    memset(p, 0, sizeof(*p));
    p->position = position;
    p->health = 100.0f;
    memset(t, 0, sizeof(*t));
    t->player = p;
    t->eye = (Vector3){ position.x, 1.6f, position.z };
    t->enemyId = -1;
}

static int RunFrames(Enemy* e, EnemyTarget* t, const WorldMap* m, int frames,
                     float* damageFlash, int* navFrames) {
    DamageIndicator inds[4];
    memset(inds, 0, sizeof(inds));
    int sawNav = 0;
    for (int i = 0; i < frames; i++) {
        float dt = 1.0f / 60.0f;
        Enemy_UpdateAll(e, 1, t, 1, m->boxes, m->boxCount, m, m->limit,
                        m->enemySpawns, m->enemySpawnCount, NULL, dt, inds, 4,
                        damageFlash, (float)i * dt);
        if (e->navActive) {
            sawNav++;
            if (navFrames) (*navFrames)++;
        }
    }
    return sawNav;
}

static void TestStuckInDeadEnd(void) {
    static WorldMap m;
    memset(&m, 0, sizeof(m));
    m.limit = 20.0f;
    AddWall(&m, (Vector3){ 0.0f, 1.5f, 2.0f }, (Vector3){ 2.0f, 1.5f, 0.5f });
    AddWall(&m, (Vector3){ -1.55f, 1.5f, -2.0f }, (Vector3){ 0.2f, 1.5f, 4.5f });
    AddWall(&m, (Vector3){ 1.55f, 1.5f, -2.0f }, (Vector3){ 0.2f, 1.5f, 4.5f });
    BuildNavGrid(&m);

    static Enemy e;
    static Player p;
    EnemyTarget t;
    SetupEnemy(&e, (Vector3){ 0.0f, 0.9f, 0.0f },
               (Vector3){ 0.0f, 0.0f, 5.0f });
    SetupTarget(&p, &t, (Vector3){ 0.0f, 0.0f, 5.0f });

    float flash = 0;
    int navFrames = 0;
    stuckLogs = 0;
    int sawNav = RunFrames(&e, &t, &m, 360, &flash, &navFrames);
    CHECK(stuckLogs >= 1, "stuck bot never logged AI STUCK");
    CHECK(sawNav > 0, "stuck bot never activated the nav fallback");
    CHECK(e.navCount > 0, "nav fallback produced no waypoints");
    CHECK(e.position.z < -1.0f,
          "bot did not follow the detour out of the dead end (z=%.2f)",
          e.position.z);
    printf("test_enemy_nav: stuck at end wall, %d nav frames, %d waypoints, "
           "detour reached z=%.1f\n", navFrames, e.navCount, e.position.z);
}

static void TestOpenFieldKeepsLocalSteering(void) {
    static WorldMap m;
    memset(&m, 0, sizeof(m));
    m.limit = 20.0f;
    BuildNavGrid(&m);

    static Enemy e;
    static Player p;
    EnemyTarget t;
    SetupEnemy(&e, (Vector3){ -4.0f, 0.9f, 0.0f }, (Vector3){ 4.0f, 0.0f, 0.0f });
    SetupTarget(&p, &t, (Vector3){ 4.0f, 0.0f, 0.0f });

    float flash = 0;
    Vector3 start = e.position;
    int sawNav = RunFrames(&e, &t, &m, 180, &flash, NULL);
    CHECK(sawNav == 0,
          "open-field bot requested A* fallback without being stuck");
    CHECK(Vector3Distance(e.position, start) > 0.1f,
          "open-field bot never moved");
    printf("test_enemy_nav: open field moved %.2fm without nav fallback\n",
           Vector3Distance(e.position, start));
}

int main(void) {
    SetTraceLogCallback(CaptureLog);
    TestStuckInDeadEnd();
    TestOpenFieldKeepsLocalSteering();
    if (failures) {
        fprintf(stderr, "test_enemy_nav: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_enemy_nav: stuck detection and A* fallback OK\n");
    return 0;
}
