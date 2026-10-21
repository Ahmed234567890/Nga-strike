#ifndef SETTINGS_H
#define SETTINGS_H

#include "common.h"

// Persistent player settings (nga.cfg in the game directory).
// Loaded in main() BEFORE InitWindow so the game starts at the chosen size.

typedef struct {
    int w, h;
} ResOption;

extern const ResOption kResOptions[];
extern const int kResCount;

typedef struct {
    int   resIndex;       // index into kResOptions
    bool  fullscreen;
    bool  vsync;          // on / uncapped off (default off = same as before)
    int   shadowQuality;  // ShadowQuality (0..3)
    int   fov;            // base field of view (degrees)
    float sens;           // mouse sensitivity
    float volume;         // master volume 0..1
    bool  pointLights;    // colored point lights on/off
    bool  fog;            // distance fog on/off
    bool  aiEnabled;      // AI enemies: spawn/respawn when on, no bots when off
    bool  infighting;     // bots also target/damage each other (chaos mode)
} Settings;

void Settings_Default(Settings* s);
void Settings_Load(Settings* s, const char* path);
void Settings_Save(const Settings* s, const char* path);

#endif
