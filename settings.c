#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const ResOption kResOptions[] = {
    { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 },
};
const int kResCount = (int)(sizeof(kResOptions)/sizeof(kResOptions[0]));

void Settings_Default(Settings* s) {
    s->resIndex = 0;
    s->fullscreen = false;
    s->vsync = false;       // uncapped by default (same as before)
    s->shadowQuality = 3;   // ULTRA (4096 PCSS + lamp shadows) - "max" default
    s->fov = 70;
    s->sens = MOUSE_SENS;
    s->volume = 1.0f;
    s->pointLights = true;
    s->fog = true;
    s->aiEnabled = true;
    s->infighting = false;  // preserve the original bots-vs-players behavior
}

void Settings_Load(Settings* s, const char* path) {
    Settings_Default(s);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char key[64];
    char val[64];
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63s %63s", key, val) != 2) continue;
        int iv = atoi(val);
        float fv = (float)atof(val);
        if (!strcmp(key, "res")) s->resIndex = iv;
        else if (!strcmp(key, "fullscreen")) s->fullscreen = (iv != 0);
        else if (!strcmp(key, "vsync")) s->vsync = (iv != 0);
        else if (!strcmp(key, "shadows")) s->shadowQuality = iv;
        else if (!strcmp(key, "fov")) s->fov = iv;
        else if (!strcmp(key, "sens")) s->sens = fv;
        else if (!strcmp(key, "volume")) s->volume = fv;
        else if (!strcmp(key, "lights")) s->pointLights = (iv != 0);
        else if (!strcmp(key, "fog")) s->fog = (iv != 0);
        else if (!strcmp(key, "ai")) s->aiEnabled = (iv != 0);
        else if (!strcmp(key, "infighting")) s->infighting = (iv != 0);
    }
    fclose(f);

    // sanitize everything so a hand-edited cfg can never break startup
    if (s->resIndex < 0 || s->resIndex >= kResCount) s->resIndex = 0;
    if (s->shadowQuality < 0 || s->shadowQuality > 3) s->shadowQuality = 3;
    if (s->fov < 50) s->fov = 50;
    if (s->fov > 110) s->fov = 110;
    s->sens = ClampF(s->sens, 0.0005f, 0.02f);
    s->volume = ClampF(s->volume, 0.0f, 1.0f);
}

void Settings_Save(const Settings* s, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "res %d\n", s->resIndex);
    fprintf(f, "fullscreen %d\n", s->fullscreen ? 1 : 0);
    fprintf(f, "vsync %d\n", s->vsync ? 1 : 0);
    fprintf(f, "shadows %d\n", s->shadowQuality);
    fprintf(f, "fov %d\n", s->fov);
    fprintf(f, "sens %.5f\n", s->sens);
    fprintf(f, "volume %.3f\n", s->volume);
    fprintf(f, "lights %d\n", s->pointLights ? 1 : 0);
    fprintf(f, "fog %d\n", s->fog ? 1 : 0);
    fprintf(f, "ai %d\n", s->aiEnabled ? 1 : 0);
    fprintf(f, "infighting %d\n", s->infighting ? 1 : 0);
    fclose(f);
}
