#ifndef WORLD_H
#define WORLD_H

#include "common.h"
#include <stdint.h>

// The world is still a list of axis-aligned boxes, but the playable bounds and
// the count are now per-map. Classic uses the original handcrafted layout;
// RANDOM and RANDOM 256 are deterministic, validated cover arenas.
#define WORLD_MAX_BOXES        384
#define WORLD_MAX_PLAYER_SPAWNS 4
#define WORLD_MAX_STREET_LAMPS   4
#define WORLD_MAX_ACCENT_LAMPS   2
#define WORLD_MAX_LAMPS         12

#define WORLD_MAX_GRASS        140
#define WORLD_MAX_ROCKS         18
#define WORLD_MAX_PEBBLES      120
#define WORLD_MAX_SITES         16

typedef enum {
    WORLD_CLASSIC = 0,
    WORLD_RANDOM = 1,
    WORLD_RANDOM_256 = 2,
    WORLD_MODE_COUNT = 3,
} WorldMode;

typedef struct {
    WorldMode mode;
    uint32_t  seed;
    float     limit;       // playable half-extent (48 for 100x100, 128 for 256x256)
    float     groundSize;  // visual ground plane size

    Box       boxes[WORLD_MAX_BOXES];
    int       boxCount;

    Vector3   playerSpawns[WORLD_MAX_PLAYER_SPAWNS];
    Vector3   enemySpawns[MAX_ENEMIES];
    int       enemySpawnCount;

    Vector3   barrelSpots[MAX_BARRELS];
    int       barrelCount;

    Vector3   pickupSpots[MAX_PICKUPS];
    PickupType pickupTypes[MAX_PICKUPS];
    int       pickupCount;

    Vector3   streetLamps[WORLD_MAX_STREET_LAMPS];
    int       streetLampCount;
    Vector3   accentLamps[WORLD_MAX_ACCENT_LAMPS];
    int       accentLampCount;

    // Ground colour details. Stored with the map so a random map's scenery and
    // collision layout always describe the same place.
    Vector3   grass[WORLD_MAX_GRASS];
    float     grassSize[WORLD_MAX_GRASS];
    int       grassCount;
    Vector3   rocks[WORLD_MAX_ROCKS];
    float     rockSize[WORLD_MAX_ROCKS];
    int       rockCount;
    Vector3   pebbles[WORLD_MAX_PEBBLES];
    float     pebbleSize[WORLD_MAX_PEBBLES];
    int       pebbleCount;

    // Random-city layout metadata. Roads and block pads are drawn from these,
    // and siteKinds lets the renderer paint parks/plazas differently without
    // storing extra geometry. Both modes rebuild identically from seed.
    int       cityCols;
    float     cityCoords[4];
    float     cityBlockHalf;
    float     cityRoadHalf;
    float     cityEdgeRoad;
    unsigned char siteKinds[WORLD_MAX_SITES];
    int       siteCount;
} WorldMap;

// Deterministically build a map. The same mode+seed always produces the exact
// same boxes, spawns, loot and scenery on host and client.
void World_Generate(WorldMap* map, WorldMode mode, uint32_t seed);

// Lit group: ground, props, boxes and generated scenery.
void World_DrawLit(const WorldMap* map, float time, Vector3 camPos);
// Collision boxes only (used by the one-time lamp shadow bake).
void World_DrawBoxes(const WorldMap* map);
// Raw collision geometry for the per-frame sun shadow pass. Random maps have
// no useful shadow casters in their decorative ground, so this skips the
// sidewalk pads, grass and rock detail while keeping every wall/prop shadow.
void World_DrawCasters(const WorldMap* map);
// Unlit lines: drawn with the default shader.
void World_DrawGrid(const WorldMap* map);
void World_DrawSky(float time, Vector3 camPos, float limit);

// Collision helpers
bool World_MoveCollides(Vector3 pos, Vector3 half, const Box* boxes, int count);
float World_RayHitBoxes(Ray ray, const Box* boxes, int count, Vector3* outPoint);
bool World_LOS(Vector3 from, Vector3 to, const Box* boxes, int count);
float World_GroundHeightAt(float x, float z, const Box* boxes, int count);

#endif
