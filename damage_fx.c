#include "damage_fx.h"

void DamageFX_Init(DamageIndicator* inds, int ic, DamageNumber* nums, int nc) {
    for (int i = 0; i < ic; i++) inds[i].timer = 0;
    for (int i = 0; i < nc; i++) nums[i].timer = 0;
}

void DamageFX_Update(DamageIndicator* inds, DamageNumber* nums, int indCount, int numCount, float dt) {
    for (int i = 0; i < indCount; i++) if (inds[i].timer > 0) inds[i].timer -= dt;
    for (int i = 0; i < numCount; i++) if (nums[i].timer > 0) nums[i].timer -= dt;
}

void DamageFX_SpawnIndicator(DamageIndicator* inds, int count, Vector3 attacker) {
    int slot = -1;
    for (int i = 0; i < count; i++) if (inds[i].timer <= 0.0f) { slot = i; break; }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < count; i++) if (inds[i].timer < inds[slot].timer) slot = i;
    }
    inds[slot].attackerPos = attacker;
    inds[slot].timer = DAMAGE_INDICATOR_TIME;
}

void DamageFX_SpawnNumber(DamageNumber* nums, int count, Vector3 worldPos, float damage, int type) {
    int slot = -1;
    for (int i = 0; i < count; i++) if (nums[i].timer <= 0.0f) { slot = i; break; }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < count; i++) if (nums[i].timer < nums[slot].timer) slot = i;
    }
    nums[slot].position = worldPos;
    nums[slot].damage = damage;
    nums[slot].timer = DAMAGE_NUMBER_TIME;
    nums[slot].type = type;
}

void DamageFX_DrawIndicators(DamageIndicator* inds, int count, Player* player, float yaw) {
    if (count <= 0) return;
    float cx = GetScreenWidth() * 0.5f;
    float cy = GetScreenHeight() * 0.5f;
    float radius = 180.0f;
    float arcHalf = 0.30f;
    for (int i = 0; i < count; i++) {
        if (inds[i].timer <= 0) continue;
        float t = inds[i].timer / DAMAGE_INDICATOR_TIME;
        Color col = { 235, 40, 40, (unsigned char)(210 * t) };
        Vector3 toE = { inds[i].attackerPos.x - player->position.x, 0,
                        inds[i].attackerPos.z - player->position.z };
        float vx = toE.x * (-cosf(yaw)) + toE.z * sinf(yaw);
        float vz = toE.x * sinf(yaw) + toE.z * cosf(yaw);
        float ang0 = atan2f(vx, vz);
        Vector2 prev = { 0 };
        for (int s = 0; s <= 14; s++) {
            float a = ang0 - arcHalf + (2 * arcHalf) * (float)s / 14.0f;
            Vector2 p = { cx + sinf(a) * radius, cy - cosf(a) * radius };
            if (s > 0) DrawLineEx(prev, p, 9.0f, col);
            prev = p;
        }
    }
}

void DamageFX_DrawNumbers(DamageNumber* nums, int count, Camera3D camera) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    for (int i = 0; i < count; i++) {
        if (nums[i].timer <= 0) continue;
        float t = nums[i].timer / DAMAGE_NUMBER_TIME;
        float rise = (1.0f - t) * 0.9f;
        Vector3 wp = { nums[i].position.x, nums[i].position.y + 0.5f + rise, nums[i].position.z };
        bool vis = false;
        Vector2 sp = WorldToScreen(wp, camera, &vis);
        if (!vis || sp.x < -80 || sp.x > sw + 80 || sp.y < -80 || sp.y > sh + 80) continue;
        unsigned char alpha = (unsigned char)(255 * (t < 0.25f ? t / 0.25f : 1.0f));
        Color col; int size; const char* txt;
        if (nums[i].type == 2)      { col = (Color){ 255, 70, 70, alpha }; size = 34; txt = TextFormat("%d  KILL", (int)nums[i].damage); }
        else if (nums[i].type == 1) { col = (Color){ 255, 215, 70, alpha }; size = 28; txt = TextFormat("%d  HEAD", (int)nums[i].damage); }
        else                        { col = (Color){ 245, 245, 245, alpha }; size = 22; txt = TextFormat("%d", (int)nums[i].damage); }
        int tw = MeasureText(txt, size);
        DrawText(txt, (int)sp.x - tw / 2 + 2, (int)sp.y + 2, size, (Color){ 0, 0, 0, alpha });
        DrawText(txt, (int)sp.x - tw / 2, (int)sp.y, size, col);
    }
}
