#include "ui.h"
#include "weapon.h"
#include <stdio.h>

void UI_DrawDamageVignette(float flash, float lowHp) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (flash > 0) {
        float a = ClampF(flash / 0.5f, 0, 1);
        // kept moderate: full-screen red at high alpha reads as a glitch
        DrawRectangle(0, 0, sw, sh, (Color){ 200, 0, 0, (unsigned char)(105 * a) });
        // directional edge pulse
        DrawRectangle(0, 0, sw, 8, (Color){ 200, 0, 0, (unsigned char)(120 * a) });
        DrawRectangle(0, sh - 8, sw, 8, (Color){ 200, 0, 0, (unsigned char)(120 * a) });
    }
    if (lowHp < 35.0f) {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 5.0f);
        unsigned char a = (unsigned char)((35.0f - lowHp) * 2.2f * pulse);
        if (a > 120) a = 120;
        DrawRectangleLinesEx((Rectangle){ 0, 0, (float)sw, (float)sh }, 26.0f, (Color){ 180, 0, 0, a });
    }
}

void UI_DrawCrosshair(float gap, bool ads, bool scoped) {
    if (scoped) return;
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    float len = ads ? 6.0f : 9.0f;
    Color c = ads ? (Color){ 140, 255, 140, 230 } : WHITE;
    float g = gap;
    DrawLine(cx - (int)(g + len), cy, cx - (int)g, cy, c);
    DrawLine(cx + (int)g, cy, cx + (int)(g + len), cy, c);
    DrawLine(cx, cy - (int)(g + len), cx, cy - (int)g, c);
    DrawLine(cx, cy + (int)g, cx, cy + (int)(g + len), c);
    DrawCircleV((Vector2){ (float)cx, (float)cy }, 1.5f, c);
}

void UI_DrawHUD(const Player* player, const Weapon* w, const Weapon* weapons,
                int curWeapon, bool reloading, float reloadT,
                int kills, int score, float gameTime, float adsBlend, bool mp) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    (void)weapons; (void)reloadT; (void)adsBlend;

    // health
    DrawRectangle(20, 20, 260, 32, (Color){ 0, 0, 0, 140 });
    DrawRectangleLines(20, 20, 260, 32, (Color){ 180, 180, 180, 200 });
    float hpFrac = ClampF(player->health / PLAYER_MAX_HEALTH, 0, 1);
    Color hpCol = { (unsigned char)(220 * (1 - hpFrac) + 60 * hpFrac),
                    (unsigned char)(60 * (1 - hpFrac) + 200 * hpFrac), 60, 255 };
    DrawRectangle(24, 24, (int)(252 * hpFrac), 24, hpCol);
    DrawText(TextFormat("%d", (int)player->health), 30, 28, 20, RAYWHITE);
    if (player->spawnProtect > 0)
        DrawText("SPAWN SHIELD", 24, 56, 16, (Color){ 140, 220, 255, 220 });

    // kills / score / time
    DrawRectangle(20, 60, 260, 56, (Color){ 0, 0, 0, 110 });
    DrawText(TextFormat("KILLS %d   SCORE %d", kills, score), 28, 66, 20, RAYWHITE);
    (void)adsBlend;
    int mm = (int)gameTime / 60, ss = (int)gameTime % 60;
    DrawText(TextFormat("TIME %02d:%02d", mm, ss), 28, 90, 18, (Color){ 200, 200, 200, 220 });

    // weapon slots
    const char* names[WEAPON_COUNT] = { "1 PISTOL", "2 AKR", "3 SNIPER" };
    for (int i = 0; i < WEAPON_COUNT; i++) {
        int x = sw - 320 + i * 100, y = sh - 160;
        Color bg = (i == curWeapon) ? (Color){ 70, 140, 90, 220 } : (Color){ 20, 22, 28, 170 };
        DrawRectangle(x, y, 92, 30, bg);
        DrawRectangleLines(x, y, 92, 30, (i == curWeapon) ? GREEN : GRAY);
        DrawText(names[i], x + 6, y + 8, 14, RAYWHITE);
    }

    DrawText(w->name, sw - 30 - MeasureText(w->name, 28), sh - 120, 28, (Color){ 200, 200, 200, 255 });
    const char* ammoTxt;
    Color ammoCol = RAYWHITE;
    if (reloading) { ammoTxt = "RELOADING"; ammoCol = (Color){ 255, 200, 80, 255 }; }
    else {
        ammoTxt = TextFormat("%d / %d", w->ammoInMag, w->ammoReserve);
        if (w->ammoInMag == 0) ammoCol = (Color){ 255, 90, 90, 255 };
        else if (w->ammoInMag <= w->magSize / 4) ammoCol = (Color){ 255, 200, 80, 255 };
    }
    int tw = MeasureText(ammoTxt, 42);
    DrawText(ammoTxt, sw - tw - 30, sh - 70, 42, ammoCol);
    if (reloading) {
        // staged reload progress bar with insert tick
        int bw = 220, bx = sw - bw - 30, by = sh - 100;
        DrawRectangle(bx, by, bw, 8, (Color){ 0, 0, 0, 160 });
        DrawRectangle(bx, by, (int)(bw * ClampF(reloadT, 0, 1)), 8, (Color){ 255, 200, 80, 255 });
        int ix = bx + (int)(bw * 0.62f);
        DrawRectangle(ix - 1, by - 3, 2, 14, WHITE);
        // phase label
        const char* phase = (reloadT < 0.35f) ? "MAG OUT" : (reloadT < 0.62f) ? "MAG IN" : "CHARGING";
        DrawText(phase, bx + bw - MeasureText(phase, 16) , by - 22, 16, (Color){ 255, 220, 130, 230 });
    }

    // LAN games have no local pause menu (it would freeze the net pump)
    DrawText(mp ? "WASD MOVE | SHIFT SPRINT | SPACE JUMP | R RELOAD | F INSPECT | RMB ADS | F6 SHADOWS"
                : "WASD MOVE | SHIFT SPRINT | SPACE JUMP | R RELOAD | F INSPECT | RMB ADS | ESC MENU | F6 SHADOWS",
             20, sh - 32, 15, (Color){ 220, 220, 220, 160 });
    DrawFPS(sw - 90, 10);
    DrawText(TextFormat("%.1f ms", GetFrameTime() * 1000.0f), sw - 110, 30, 16, (Color){ 200, 200, 200, 200 });
}

void UI_DrawPauseMenu(bool* paused, bool* shouldQuit, Game* g) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    enum { MR_RES, MR_FULL, MR_SHADOW, MR_FOV, MR_SENS, MR_VOL, MR_LIGHTS, MR_FOG,
           MR_RESUME, MR_QUIT, MR_COUNT };
    static int msel = 0;
    const int rowH = 36;
    float panelW = 640.0f;
    float panelH = 92.0f + (float)MR_COUNT * (float)rowH + 30.0f;
    Rectangle panel = { (sw - panelW) / 2.0f, (sh - panelH) / 2.0f, panelW, panelH };
    float rowX = panel.x + 24.0f, rowW = panel.width - 48.0f;
    float rowY0 = panel.y + 84.0f;

    DrawRectangleRec(panel, (Color){ 26, 28, 34, 246 });
    DrawRectangleLinesEx(panel, 3.0f, (Color){ 95, 100, 112, 255 });
    const char* title = "SETTINGS";
    DrawText(title, (int)(panel.x + (panelW - MeasureText(title, 44)) / 2), (int)panel.y + 26, 44, RAYWHITE);
    DrawRectangle((int)panel.x + 24, (int)panel.y + 76, (int)panelW - 48, 2, (Color){ 95, 100, 112, 255 });

    // ---- input ----
    Vector2 mouse = GetMousePosition();
    int hoverRow = -1;
    for (int i = 0; i < MR_COUNT; i++) {
        Rectangle r = { rowX, rowY0 + (float)i * rowH, rowW, (float)rowH - 4 };
        if (CheckCollisionPointRec(mouse, r)) hoverRow = i;
    }
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool activate = IsKeyPressed(KEY_ENTER);
    int act = 0; // -1 / +1 adjustment for the selected value row
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) msel = (msel + MR_COUNT - 1) % MR_COUNT;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) msel = (msel + 1) % MR_COUNT;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A)) act = -1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D)) act = +1;
    if (hoverRow >= 0 && clicked) {
        msel = hoverRow;
        if (hoverRow == MR_RESUME || hoverRow == MR_QUIT) activate = true;
        else {
            Rectangle r = { rowX, rowY0 + (float)hoverRow * rowH, rowW, (float)rowH - 4 };
            act = (mouse.x < r.x + r.width * 0.5f) ? -1 : +1;
        }
    }

    // ---- apply ----
    bool changed = false;
    if (activate && msel == MR_RESUME) {
        *paused = false;
        DisableCursor();
        SetMasterVolume(g->masterVol);
    } else if (activate && msel == MR_QUIT) {
        *shouldQuit = true;
    } else if (act != 0 || (activate && msel < MR_RESUME)) {
        if (activate && msel < MR_RESUME) act = +1; // ENTER cycles value rows
        switch (msel) {
        case MR_RES:
            g->set.resIndex = (g->set.resIndex + act + kResCount) % kResCount;
            if (!IsWindowFullscreen())
                SetWindowSize(kResOptions[g->set.resIndex].w, kResOptions[g->set.resIndex].h);
            changed = true;
            break;
        case MR_FULL:
            g->set.fullscreen = !g->set.fullscreen;
            if (g->set.fullscreen != IsWindowFullscreen()) ToggleFullscreen();
            if (!IsWindowFullscreen())
                SetWindowSize(kResOptions[g->set.resIndex].w, kResOptions[g->set.resIndex].h);
            changed = true;
            break;
        case MR_SHADOW:
            g->set.shadowQuality = (g->set.shadowQuality + act + 4) % 4;
            Shadow_SetQuality(&g->shadow, g->set.shadowQuality);
            changed = true;
            break;
        case MR_FOV:
            g->set.fov = (int)ClampF((float)(g->set.fov + act * 2), 50.0f, 110.0f);
            changed = true;
            break;
        case MR_SENS:
            g->set.sens = ClampF(g->set.sens + act * 0.0002f, 0.0005f, 0.02f);
            g->sens = g->set.sens;
            changed = true;
            break;
        case MR_VOL:
            g->set.volume = ClampF(g->set.volume + act * 0.05f, 0.0f, 1.0f);
            g->masterVol = g->set.volume;
            SetMasterVolume(g->masterVol * 0.25f);
            changed = true;
            break;
        case MR_LIGHTS:
            g->set.pointLights = !g->set.pointLights;
            changed = true;
            break;
        case MR_FOG:
            g->set.fog = !g->set.fog;
            changed = true;
            break;
        }
    }
    if (changed) Settings_Save(&g->set, "nga.cfg");

    // ---- draw rows ----
    const char* labels[MR_COUNT] = {
        "RESOLUTION", "FULLSCREEN", "SHADOWS", "FIELD OF VIEW",
        "SENSITIVITY", "VOLUME", "POINT LIGHTS", "DISTANCE FOG",
        "RESUME", "QUIT"
    };
    static const char* shadowNames[4] = { "OFF", "LOW", "HIGH", "ULTRA" };
    for (int i = 0; i < MR_COUNT; i++) {
        Rectangle r = { rowX, rowY0 + (float)i * rowH, rowW, (float)rowH - 4 };
        bool selRow = (i == msel);
        bool isButton = (i == MR_RESUME || i == MR_QUIT);
        Color bg = selRow ? (Color){ 52, 58, 70, 255 } : (Color){ 34, 37, 44, 235 };
        if (isButton) {
            bg = (i == MR_QUIT)
               ? (selRow ? (Color){ 170, 55, 55, 255 } : (Color){ 90, 30, 30, 255 })
               : (selRow ? (Color){ 70, 140, 90, 255 } : (Color){ 45, 85, 60, 255 });
        }
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1.5f, selRow ? (Color){ 130, 200, 150, 255 } : (Color){ 70, 74, 84, 255 });

        if (isButton) {
            DrawText(labels[i], (int)(r.x + (r.width - MeasureText(labels[i], 24)) / 2), (int)r.y + 5, 24, RAYWHITE);
            continue;
        }

        char val[64];
        switch (i) {
        case MR_RES: snprintf(val, sizeof(val), "%d x %d", kResOptions[g->set.resIndex].w, kResOptions[g->set.resIndex].h); break;
        case MR_FULL: snprintf(val, sizeof(val), "%s", IsWindowFullscreen() ? "ON (MONITOR)" : "OFF"); break;
        case MR_SHADOW:
            snprintf(val, sizeof(val), "%s",
                     g->shadow.available ? shadowNames[g->set.shadowQuality] : "N/A");
            break;
        case MR_FOV: snprintf(val, sizeof(val), "%d", g->set.fov); break;
        case MR_SENS: snprintf(val, sizeof(val), "%.4f", g->set.sens); break;
        case MR_VOL: snprintf(val, sizeof(val), "%d%%", (int)(g->set.volume * 100)); break;
        case MR_LIGHTS: snprintf(val, sizeof(val), "%s", g->set.pointLights ? "ON" : "OFF"); break;
        case MR_FOG: snprintf(val, sizeof(val), "%s", g->set.fog ? "ON" : "OFF"); break;
        default: val[0] = 0; break;
        }
        DrawText(labels[i], (int)r.x + 14, (int)r.y + 6, 20, (Color){ 215, 218, 226, 255 });
        int vw = MeasureText(val, 20);
        int vx = (int)(r.x + r.width - 92 - vw);
        Color vc = selRow ? (Color){ 140, 255, 170, 255 } : (Color){ 235, 238, 245, 255 };
        DrawText(val, vx, (int)r.y + 6, 20, vc);
        DrawText("<", (int)(r.x + r.width - 84), (int)r.y + 6, 20, selRow ? WHITE : (Color){ 140, 144, 152, 255 });
        DrawText(">", (int)(r.x + r.width - 34), (int)r.y + 6, 20, selRow ? WHITE : (Color){ 140, 144, 152, 255 });
    }

    DrawText("UP/DOWN SELECT   LEFT/RIGHT CHANGE   ENTER ACTIVATE   ESC RESUME",
             (int)panel.x + 24, (int)(panel.y + panelH - 22), 15, (Color){ 170, 175, 185, 230 });
    DrawText(TextFormat("FPS %d", GetFPS()), (int)(panel.x + panelW - 150), (int)panel.y + 16, 16,
             (Color){ 160, 200, 160, 230 });
}

void UI_DrawHeadshotBanner(float timer, int streak, int totalHeadshots) {
    if (timer <= 0) return;
    int sw = GetScreenWidth();
    float a = ClampF(timer / 1.4f, 0, 1);
    unsigned char alpha = (unsigned char)(255 * a);
    const char* txt = "HEADSHOT!";
    int size = 46;
    int tw = MeasureText(txt, size);
    int cx = sw / 2 - tw / 2;
    int cy = 120;
    // glow backdrop
    DrawRectangle(cx - 24, cy - 10, tw + 48, 64, (Color){ 0, 0, 0, (unsigned char)(120 * a) });
    DrawRectangleLines(cx - 24, cy - 10, tw + 48, 64, (Color){ 255, 215, 70, alpha });
    DrawText(txt, cx + 2, cy + 2, size, (Color){ 0, 0, 0, alpha });
    DrawText(txt, cx, cy, size, (Color){ 255, 215, 70, alpha });
    if (streak >= 2) {
        const char* st = TextFormat("x%d STREAK", streak);
        int s2 = 22;
        int sw2 = MeasureText(st, s2);
        DrawText(st, sw / 2 - sw2 / 2 + 1, cy + 51 + 1, s2, (Color){ 0, 0, 0, alpha });
        DrawText(st, sw / 2 - sw2 / 2, cy + 51, s2, (Color){ 255, 140, 60, alpha });
    }
    const char* tot = TextFormat("HEADSHOTS: %d", totalHeadshots);
    int s3 = 16;
    DrawText(tot, sw / 2 - MeasureText(tot, s3) / 2, cy + 78, s3, (Color){ 220, 220, 220, alpha });
}

// LAN multiplayer overlay: status line, scoreboard, kill feed, name tags and
// the respawn overlay. No-op in single-player (net->active == false).
void UI_DrawNetHud(const NetState* net, Camera3D cam, float gameTime, int myKills) {
    if (!net->active) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    (void)gameTime;

    // ---- status line (top center) ----
    const char* status = NULL;
    Color statusCol = (Color){ 220, 220, 220, 230 };
    if (net->mode == NET_HOST) {
        if (!Net_HasPeers(net)) {
            static char buf[64];
            snprintf(buf, sizeof(buf), "HOSTING - WAITING FOR PLAYERS (UDP %d)", net->port);
            status = buf;
            statusCol = (Color){ 150, 220, 150, 230 };
        }
    } else {
        if (net->serverFull) { status = "SERVER FULL"; statusCol = (Color){ 255, 140, 120, 230 }; }
        else if (!net->connected) status = "CONNECTING TO HOST...";
        else if (net->hostLost) { status = "HOST CONNECTION LOST"; statusCol = (Color){ 255, 140, 120, 230 }; }
    }
    if (status) {
        int tw = MeasureText(status, 18);
        DrawRectangle(sw / 2 - tw / 2 - 10, 10, tw + 20, 26, (Color){ 0, 0, 0, 120 });
        DrawText(status, sw / 2 - tw / 2, 14, 18, statusCol);
    }

    // ---- scoreboard (top right, under the frame time) ----
    int y = 54;
    DrawRectangle(sw - 230, y - 6, 210, 26 + 26 * (NET_MAX_PLAYERS - 1), (Color){ 0, 0, 0, 110 });
    Color selfCol = Net_PlayerColor(net->myId > 0 ? net->myId : 1);
    DrawRectangle(sw - 224, y, 12, 12, selfCol);
    DrawText(TextFormat("%s  K:%d", net->myName, myKills), sw - 206, y - 2, 18,
             net->localDead ? (Color){ 160, 160, 160, 230 } : RAYWHITE);
    int row = 1;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &net->peers[i];
        if (!p->used || p->id == net->myId) continue;
        int yy = y + 24 * row;
        DrawRectangle(sw - 224, yy, 12, 12, Net_PlayerColor(p->id));
        bool alive = (p->flags & NETF_ALIVE) != 0;
        Color c = alive ? (Color){ 215, 218, 226, 230 } : (Color){ 150, 120, 120, 200 };
        DrawText(TextFormat("%s  K:%d%s", p->name[0] ? p->name : "PLAYER", p->kills,
                            alive ? "" : "  [DEAD]"), sw - 206, yy - 2, 18, c);
        row++;
    }

    // ---- kill feed (right, below the scoreboard) ----
    int fy = y + 26 * NET_MAX_PLAYERS + 14;
    for (int i = 0; i < NET_FEED_LINES; i++) {
        const NetFeedLine* f = &net->feed[i];
        if (f->timer <= 0 || !f->text[0]) continue;
        float a = ClampF(f->timer / 1.2f, 0.0f, 1.0f);
        unsigned char al = (unsigned char)(a * 235);
        int tw = MeasureText(f->text, 18);
        DrawRectangle(sw - 30 - tw - 8, fy, tw + 16, 24, (Color){ 0, 0, 0, (unsigned char)(110 * a) });
        DrawText(f->text, sw - 30 - tw, fy + 3, 18, (Color){ 255, 235, 200, al });
        fy += 26;
    }

    // ---- name tags over remote players ----
    if (!net->localDead) {
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            const NetPeer* p = &net->peers[i];
            if (!p->used || p->id == net->myId) continue;
            if (!(p->flags & NETF_ALIVE)) continue;
            Vector3 head = { p->pos.x, p->pos.y + 2.1f, p->pos.z };
            float dist = Vector3Distance(head, cam.position);
            if (dist > 55.0f) continue;
            bool vis = false;
            Vector2 s = WorldToScreen(head, cam, &vis);
            if (!vis || s.x < -40 || s.x > sw + 40 || s.y < -20 || s.y > sh + 20) continue;
            unsigned char al = (unsigned char)(ClampF(1.4f - dist / 55.0f, 0.25f, 1.0f) * 220);
            const char* nm = p->name[0] ? p->name : "PLAYER";
            int tw = MeasureText(nm, 16);
            DrawRectangle((int)s.x - tw / 2 - 6, (int)s.y - 20, tw + 12, 22, (Color){ 0, 0, 0, (unsigned char)(al * 0.55f) });
            Color col = Net_PlayerColor(p->id);
            col.a = al;
            DrawText(nm, (int)s.x - tw / 2, (int)s.y - 17, 16, col);
        }
    }

    // ---- respawn overlay ----
    if (net->localDead) {
        DrawRectangle(0, 0, sw, sh, (Color){ 40, 0, 0, 130 });
        const char* t1 = "YOU DIED";
        const char* t2 = TextFormat("RESPAWNING IN %.1f", ClampF(net->respawnTimer, 0, 99));
        DrawText(t1, sw / 2 - MeasureText(t1, 54) / 2, sh / 2 - 70, 54, (Color){ 255, 80, 60, 240 });
        DrawText(t2, sw / 2 - MeasureText(t2, 28) / 2, sh / 2 + 4, 28, (Color){ 235, 225, 220, 230 });
    }
}
