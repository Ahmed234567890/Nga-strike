#ifndef FX_H
#define FX_H

#include "common.h"

void FX_Init(Particle* p, int pc, Tracer* t, int tc, Decal* d, int dc);
void FX_SpawnSpark(Particle* p, int pc, Vector3 pos, int n);
void FX_SpawnBlood(Particle* p, int pc, Vector3 pos, int n);
void FX_SpawnHeadBlood(Particle* p, int pc, Vector3 pos, int n);
void FX_SpawnSmoke(Particle* p, int pc, Vector3 pos, int n);
void FX_SpawnMuzzleSmoke(Particle* p, int pc, Vector3 pos, Vector3 dir, int n);
void FX_SpawnPickupBurst(Particle* p, int pc, Vector3 pos, Color color);
void FX_SpawnCasing(Particle* p, int pc, Vector3 pos, Vector3 right);
void FX_SpawnHelmetPop(Particle* p, int pc, Vector3 pos);
void FX_SpawnExplosion(Particle* p, int pc, Vector3 pos, int power);
void FX_SpawnDustMote(Particle* p, int pc, Vector3 center, float range);
void FX_SpawnDirtKick(Particle* p, int pc, Vector3 pos, int n);
void FX_AddTracer(Tracer* t, int tc, Vector3 a, Vector3 b, Color color);
void FX_AddDecal(Decal* d, int dc, Vector3 pos, float size, Color color, int* cursor);
void FX_Update(Particle* p, int pc, Tracer* t, int tc, Decal* d, int dc, float dt);
void FX_Draw(const Particle* p, int pc, const Tracer* t, int tc, const Decal* d, int dc);
void FX_DrawShockwaves(const Tracer* t, int tc);

#endif
