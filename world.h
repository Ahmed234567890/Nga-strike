#ifndef WORLD_H
#define WORLD_H

#include "common.h"

#define WORLD_MAX_BOXES 128

void World_Init(Box* boxes, int* count);
// Lit group: ground, props, boxes. Rendered into the shadow map (pass 1)
// and with the lambert+shadow program (pass 2).
void World_DrawLit(const Box* boxes, int count, float time, Vector3 camPos);
// Unlit lines: drawn with the default shader (grid normals are stale state
// and would flicker under lighting).
void World_DrawGrid(void);
void World_DrawSky(float time, Vector3 camPos);

// Collision helpers
bool World_MoveCollides(Vector3 pos, Vector3 half, const Box* boxes, int count);
float World_RayHitBoxes(Ray ray, const Box* boxes, int count, Vector3* outPoint);
bool World_LOS(Vector3 from, Vector3 to, const Box* boxes, int count);
float World_GroundHeightAt(float x, float z, const Box* boxes, int count);

#endif
