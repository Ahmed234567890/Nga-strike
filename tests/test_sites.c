#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// In-repo regression tests for random-city site placement: every block must
// receive a site, all kinds stay in range, and each placed site must own real
// collision geometry inside its block.

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

static void TestMode(WorldMode mode) {
    static WorldMap m;
    int large = (mode == WORLD_RANDOM_256);
    int cols = large ? 4 : 3;
    unsigned char kindsSeen[9];
    memset(kindsSeen, 0, sizeof(kindsSeen));
    int minPerSite = 1 << 30;
    int missing = 0;
    int placedTotal = 0;

    for (int i = 0; i < 80; i++) {
        uint32_t seed = 7u + (uint32_t)i * 2246822519u;
        World_Generate(&m, mode, seed);
        CHECK(m.cityCols == cols, "mode %d seed %u: cityCols %d", (int)mode,
              seed, m.cityCols);
        CHECK(m.siteCount == cols * cols, "mode %d seed %u: siteCount %d",
              (int)mode, seed, m.siteCount);
        CHECK(fabsf(m.cityBlockHalf - (large ? 25.0f : 11.0f)) < 0.01f,
              "mode %d seed %u: block half %.2f", (int)mode, seed,
              m.cityBlockHalf);
        CHECK(fabsf(m.cityRoadHalf - (large ? 7.0f : 4.0f)) < 0.01f,
              "mode %d seed %u: road half %.2f", (int)mode, seed,
              m.cityRoadHalf);

        for (int gz = 0; gz < cols; gz++) {
            float z = m.cityCoords[gz];
            CHECK(fabsf(z) < m.limit, "mode %d seed %u: site row %.1f out",
                  (int)mode, seed, z);
            if (gz > 0)
                CHECK(m.cityCoords[gz] > m.cityCoords[gz - 1],
                      "mode %d seed %u: site rows not ordered", (int)mode, seed);
        }

        for (int s = 0; s < m.siteCount; s++) {
            unsigned char kind = m.siteKinds[s];
            CHECK(kind <= 8, "mode %d seed %u: site %d kind %u", (int)mode, seed,
                  s, (unsigned)kind);
            if (kind == 255) {
                missing++;
                continue;
            }
            kindsSeen[kind]++;
            placedTotal++;
            int gx = s % cols;
            int gz = s / cols;
            float cx = m.cityCoords[gx];
            float cz = m.cityCoords[gz];
            int boxes = 0;
            for (int b = 0; b < m.boxCount; b++) {
                const Box* box = &m.boxes[b];
                float bottom = box->center.y - box->halfExtents.y;
                float top = box->center.y + box->halfExtents.y;
                if (fabsf(box->center.x - cx) < m.cityBlockHalf + 2.0f &&
                    fabsf(box->center.z - cz) < m.cityBlockHalf + 2.0f &&
                    bottom < 1.0f && top > 0.35f)
                    boxes++;
            }
            if (boxes < minPerSite) minPerSite = boxes;
            CHECK(boxes >= 2, "mode %d seed %u: site %d kind %u has %d boxes",
                  (int)mode, seed, s, (unsigned)kind, boxes);
        }

        // Site kinds must survive a rebuild from the same seed.
        static WorldMap again;
        World_Generate(&again, mode, seed);
        CHECK(again.siteCount == m.siteCount &&
              memcmp(again.siteKinds, m.siteKinds, (size_t)m.siteCount) == 0,
              "mode %d seed %u: site kinds are not deterministic", (int)mode,
              seed);
    }

    int expected = cols * cols * 80;
    CHECK(placedTotal == expected,
          "mode %d: only %d/%d sites were placed", (int)mode, placedTotal,
          expected);
    CHECK(missing == 0, "mode %d: %d sites fell back to empty", (int)mode,
          missing);
    CHECK(minPerSite >= 2, "mode %d: site with only %d boxes", (int)mode,
          minPerSite);
    for (int k = 0; k < 8; k++)
        CHECK(kindsSeen[k] > 0, "mode %d: site kind %d never appeared",
              (int)mode, k);
    printf("test_sites: mode %d: 80 seeds, %d sites, all kinds 0..7 seen, "
           "min boxes/site %d\n", (int)mode, placedTotal, minPerSite);
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    TestMode(WORLD_RANDOM);
    TestMode(WORLD_RANDOM_256);
    if (failures) {
        fprintf(stderr, "test_sites: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_sites: site coverage and determinism OK\n");
    return 0;
}
