#include "raylib.h"
#include "game.h"
#include "settings.h"
#include "ui.h"
#include "world.h"
#include "pickup.h"
#include "enemy.h"
#include "fx.h"
#include "net.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintUsage(void) {
    TraceLog(LOG_INFO, "usage: nga_strike [--host [port]] [--join IP[:port]] [--name NAME] [--no-upnp] [--no-ai] [--world MODE] [--seed N]");
    TraceLog(LOG_INFO, "  no flags        -> single-player (exactly as before)");
    TraceLog(LOG_INFO, "  --host          -> host (world authority) on UDP 7777; opens the");
    TraceLog(LOG_INFO, "                     port on the router via UPnP when possible");
    TraceLog(LOG_INFO, "  --join 1.2.3.4  -> client, connects to host (default port 7777)");
    TraceLog(LOG_INFO, "  --no-upnp       -> host only: don't ask the router to open the port");
    TraceLog(LOG_INFO, "  --no-ai         -> start with no AI bots (pure PvP / empty arena)");
    TraceLog(LOG_INFO, "  --world MODE    -> startup map: classic, random100 or random256");
    TraceLog(LOG_INFO, "  --seed N        -> deterministic random-map seed (1..4294967295)");
    TraceLog(LOG_INFO, "  --seed/--world are startup overrides; NEW MAP still owns live remaps");
}

int main(int argc, char** argv) {
    // ---- CLI: multiplayer flags (see PrintUsage) ----
    int netMode = NET_OFF;
    char netAddr[64] = "";
    int netPort = NET_PORT_DEFAULT;
    char netName[16] = "";
    bool noAI = false;
    int worldOverride = -1;
    uint32_t seedOverride = 0;
    bool haveSeedOverride = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0) {
            netMode = NET_HOST;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                netPort = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--join") == 0) {
            if (i + 1 >= argc) {
                TraceLog(LOG_WARNING, "--join needs an IP[:port]; staying single-player");
                netMode = NET_OFF;
            } else {
                netMode = NET_CLIENT;
                snprintf(netAddr, sizeof(netAddr), "%s", argv[++i]);
                char* colon = strchr(netAddr, ':');
                if (colon) { *colon = 0; netPort = atoi(colon + 1); }
                if (!netAddr[0]) {
                    TraceLog(LOG_WARNING, "--join needs an IP[:port]; staying single-player");
                    netMode = NET_OFF;
                }
            }
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            snprintf(netName, sizeof(netName), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--no-upnp") == 0) {
            Net_SetUPnPEnabled(false);
        } else if (strcmp(argv[i], "--no-ai") == 0) {
            noAI = true;
        } else if (strcmp(argv[i], "--world") == 0 && i + 1 < argc) {
            const char* w = argv[++i];
            if (!strcmp(w, "0") || !strcmp(w, "classic"))
                worldOverride = WORLD_CLASSIC;
            else if (!strcmp(w, "1") || !strcmp(w, "100") ||
                     !strcmp(w, "random") || !strcmp(w, "random100"))
                worldOverride = WORLD_RANDOM;
            else if (!strcmp(w, "2") || !strcmp(w, "256") || !strcmp(w, "random256"))
                worldOverride = WORLD_RANDOM_256;
            else
                TraceLog(LOG_WARNING, "--world '%s' is not classic/random100/random256; keeping nga.cfg", w);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            char* end = NULL;
            unsigned long v = strtoul(argv[++i], &end, 10);
            if (!end || *end || v == 0 || v > 4294967295UL) {
                TraceLog(LOG_WARNING, "--seed must be an integer from 1 to 4294967295; keeping nga.cfg");
            } else {
                seedOverride = (uint32_t)v;
                haveSeedOverride = true;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage();
            return 0;
        }
    }
    Net_Configure(netMode, netAddr, netPort, netName);

    // Load settings before the window exists so we start at the chosen size.
    Settings st;
    Settings_Load(&st, "nga.cfg");
    if (worldOverride >= 0) st.worldMode = worldOverride;
    if (haveSeedOverride) st.worldSeed = seedOverride;
    if (noAI) st.aiEnabled = false; // CLI override, not persisted
    int winW = kResOptions[st.resIndex].w;
    int winH = kResOptions[st.resIndex].h;

    // Uncapped by default (no VSYNC flag, target 0); the settings menu can
    // enable VSYNC, which we honor here at window creation.
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE |
                   (st.vsync ? FLAG_VSYNC_HINT : 0));
    InitWindow(winW, winH, "Nga-Strike");
    if (st.fullscreen) ToggleFullscreen();
    InitAudioDevice();
    SetExitKey(KEY_NULL);
    DisableCursor();
    SetTargetFPS(0); // uncapped
    SetRandomSeed((unsigned int)time(NULL));

    Game g;
    Game_Init(&g, &st);
    TraceLog(LOG_INFO, "Nga-Strike start: %dx%d windowed=%d shadows=%d vsync=%d world=%d seed=%u",
             GetScreenWidth(), GetScreenHeight(), !IsWindowFullscreen(),
             st.shadowQuality, st.vsync ? 1 : 0, (int)g.world.mode, g.world.seed);
    // apply the resolved startup settings to live state
    g.sens = st.sens;
    g.masterVol = st.volume;
    Shadow_SetQuality(&g.shadow, st.shadowQuality);
    SetMasterVolume(st.volume);

    while (!WindowShouldClose() && !g.shouldQuit) {
        // keep viewRT matched to window size (resizable)
        if (g.viewRTok && (g.viewRT.texture.width != GetScreenWidth() ||
                           g.viewRT.texture.height != GetScreenHeight())) {
            UnloadRenderTexture(g.viewRT);
            g.viewRT = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
            g.viewRTok = (g.viewRT.id > 0);
            if (g.viewRTok) SetTextureFilter(g.viewRT.texture, TEXTURE_FILTER_BILINEAR);
        }
        float dt = GetFrameTime();
        Game_Update(&g, dt);
        if (g.shouldQuit) break;
        // Single draw path (Game_Draw handles pause overlay + barrels + fx).
        // Previously a duplicated pause branch omitted barrels/vignette and
        // caused pop-in when toggling pause.
        Game_Draw(&g);
    }

    Game_Unload(&g);
    CloseAudioDevice();
    EnableCursor();
    CloseWindow();
    return 0;
}
