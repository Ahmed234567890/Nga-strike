#ifndef SHADOWS_H
#define SHADOWS_H

#include "common.h"

// Real-time shadow mapping, v3.
// Two programs share the standard rlgl vertex layout (positions and normals
// arrive pre-transformed in world space), so ordinary DrawCube/DrawSphere/
// DrawCylinder calls just work under them:
//
//   pass 1 (into shadowRT): depth-pack program, light POV, casters only.
//   pass 2 (main view):     sun+shadow+lights+fog program for the lit group
//                           (world boxes/props/barrels/pickups/enemies).
//
// v3 upgrades:
//   - ULTRA is now 4096 with 25-tap rotated PCSS + wider penumbra range
//   - baked point-light shadows: every static lamp (barrel lamps, street
//     lamps, accent lights) gets a 6-face shadow "cube" baked ONCE into a
//     2D atlas (3 tiles x N rows). The lit shader samples the nearest two
//     lamps' faces -> small lights really cast shadows at zero per-frame
//     caster cost.
//   - point lights carry a lampIndex so the shader knows which light maps
//     to which baked atlas lamp.
//
// v2 features retained:
//   - camera-following ortho volume, texel-snapped (sharp + no shimmer)
//   - quality tiers: 1024 9-tap PCF / 2048 PCSS / 4096 PCSS
//   - up to 8 colored point lights (lamps, barrels, muzzle flashes)
//   - per-pixel distance fog matching the CPU sky-haze curve
//   - hemisphere ambient (sky/ground) + gentle sun specular + color grade
//
// If either program fails to compile, shadows silently stay disabled and the
// game renders exactly as before.

#define SHADOW_MAX_LIGHTS 8
#define SHADOW_MAX_BAKED_LAMPS 12
#define SHADOW_DYN_LAMPS 2        // nearest lamps get per-frame actor shadows
#define SHADOW_DYN_TILE  128      // px per cube face in the dynamic atlas
#define SHADOW_DYN_RANGE 10.0f    // m: actors only shadow lamps closer than this

typedef enum {
    SHADOW_QUAL_OFF   = 0,
    SHADOW_QUAL_LOW   = 1,   // 1024, hard-ish 9-tap PCF
    SHADOW_QUAL_HIGH  = 2,   // 2048, PCSS
    SHADOW_QUAL_ULTRA = 3,   // 4096, PCSS + wider penumbra
} ShadowQuality;

typedef struct {
    Vector3 pos;
    Vector3 color;   // color * intensity (can exceed 1)
    float   radius;  // world-space falloff radius
    int     lampIndex; // >=0: baked lamp atlas index (static occluders); -1 none
    int     dynSlot;   // >=0: dynamic actor atlas slot this frame (0/1); -1 none
} PointLight;

typedef struct {
    bool  enabled;        // user toggle (F6 / settings)
    bool  available;      // programs + render target alive
    int   quality;        // ShadowQuality
    int   mapSize;
    float halfSize;       // ortho half-extent in world units
    float focusDist;      // how far ahead of the camera the volume leans
    bool  fogEnabled;
    RenderTexture2D rt;
    unsigned int depthProg;
    unsigned int litProg;
    int depthLocs[32];
    int litLocs[32];
    int locLightVP;
    int locSunDir;
    int locSunColor;
    int locAmbSky;
    int locAmbGnd;
    int locCamPos;
    int locShadowMap;
    int locTexel;
    int locBias;      // constant depth bias (float, packed-depth units)
    int locNormOff;   // vertex normal offset in world units (float)
    int locSoft;      // int: 0 = PCF9, 1 = PCSS
    int locFogOn;
    int locFogColor;
    int locFogStart;
    int locFogRange;
    int locFogMax;
    int locLightCount;
    int locLightPos;
    int locLightColor;
    int locLightRadius;
    int locSpecK;
    int locSat;
    int locCon;
    int locLampAtlas;
    int locLampMap;
    int locLampTiles;
    int locLampNear;
    int locLampFar;
    int locPenK;
    int locPenMax;
    int locBlkStride;
    bool  lampShadows;                 // atlas alive + baked
    RenderTexture2D lampRT;
    int   lampCount;
    float lampNear, lampFar;
    Vector3 lampPos[SHADOW_MAX_BAKED_LAMPS];
    // v3.1 dynamic actor shadows: the <=2 nearest lamps re-render 6 faces per
    // frame with moving bodies only (player/enemies); the lit shader takes
    // min(baked, dynamic) so small lights shadow moving actors too.
    RenderTexture2D lampDynRT;
    int   lampDynLamps[SHADOW_DYN_LAMPS]; // lampIndex per slot, -1 = unused
    bool  lampDynReady;                   // atlas holds this frame's actors
    int   locLampDynTex;
    int   locLampDynTiles;
    int   locLampDynSlot;
    Matrix lightView;
    Matrix lightProj;
    Matrix lightVP;   // rlgl order: view * proj (matches internal MVP math)
    Matrix savedProj; // restored verbatim after the caster pass
    Matrix savedView;
    Vector3 sunDir;   // world-space direction TO the sun (normalized)
} ShadowMap;

void Shadow_Init(ShadowMap* s);
void Shadow_Unload(ShadowMap* s);

// Quality tier (recreates the map if needed). quality 0 disables shadows.
void Shadow_SetQuality(ShadowMap* s, int quality);

// Bake static point-light shadows: each lamp gets 6 perspective faces
// (90 deg FOV) rendered once into a 2D atlas. The caller supplies the lamp
// positions and a caster-draw callback (world geometry only -- the lamps'
// own fixtures/barrels must NOT be drawn or they would occlude the pool
// they sit in). Requires the programs to be available. On failure lamp
// shadows silently stay off.
typedef void (*ShadowCasterFn)(void* user);
void Shadow_BakeLamps(ShadowMap* s, const Vector3* lampPos, int count,
                      ShadowCasterFn drawCasters, void* user);

// Per-frame dynamic pass: re-render the actor shadow atlas for the two
// nearest lamps (<= SHADOW_DYN_RANGE from the camera) with drawActors
// (moving bodies only -- the static world is in the baked atlas, and barrels
// must be excluded or a lamp would shade its own fixture/body). Call once
// per frame after the sun caster pass and before Shadow_ApplyLit.
void Shadow_UpdateLampDyn(ShadowMap* s, Vector3 camPos, const Vector3* lampPos,
                          int lampCount, ShadowCasterFn drawActors, void* user);

// Dynamic slot (0..SHADOW_DYN_LAMPS-1) currently held by a baked lamp index,
// or -1. Store it in PointLight.dynSlot before Shadow_ApplyLit.
int Shadow_LampDynSlot(const ShadowMap* s, int lampIndex);

// Re-center the shadow volume ahead of the camera (texel-snapped so shadow
// edges do not shimmer while walking). Call once per frame before the pass.
void Shadow_UpdateLight(ShadowMap* s, Vector3 camPos, Vector3 camForward);

// Pass 1: render casters into the map. Caller draws the SAME world set as
// the lit group (boxes/props/barrels/pickups/enemies/player silhouette)
// between these calls.
void Shadow_BeginCasterPass(ShadowMap* s);
void Shadow_EndCasterPass(ShadowMap* s);

// Pass 2: bind the lit program (+per-frame uniforms: light matrices, shadow
// map, camera position, point lights) for the world group; call
// Shadow_Release afterwards to restore the default shader.
void Shadow_ApplyLit(ShadowMap* s, Vector3 camPos, const PointLight* lights, int lightCount);
void Shadow_Release(ShadowMap* s);

#endif
