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
    TraceLog(LOG_INFO, "usage: nga_strike [--host [port]] [--join IP[:port]] [--name NAME]");
    TraceLog(LOG_INFO, "  no flags        -> single-player (exactly as before)");
    TraceLog(LOG_INFO, "  --host          -> LAN host (world authority) on UDP 7777");
    TraceLog(LOG_INFO, "  --join 1.2.3.4  -> LAN client, connects to host (default port 7777)");
}

int main(int argc, char** argv) {
    // ---- CLI: multiplayer flags (see PrintUsage) ----
    int netMode = NET_OFF;
    char netAddr[64] = "";
    int netPort = NET_PORT_DEFAULT;
    char netName[16] = "";
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
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage();
            return 0;
        }
    }
    Net_Configure(netMode, netAddr, netPort, netName);

    // Load settings before the window exists so we start at the chosen size.
    Settings st;
    Settings_Load(&st, "nga.cfg");
    int winW = kResOptions[st.resIndex].w;
    int winH = kResOptions[st.resIndex].h;

    // Unlimited FPS: no VSYNC flag, target 0
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(winW, winH, "Nga-Strike");
    if (st.fullscreen) ToggleFullscreen();
    InitAudioDevice();
    SetExitKey(KEY_NULL);
    DisableCursor();
    SetTargetFPS(0); // uncapped
    SetRandomSeed((unsigned int)time(NULL));

    Game g;
    Game_Init(&g);
    TraceLog(LOG_INFO, "Nga-Strike start: %dx%d windowed=%d shadows=%d", GetScreenWidth(), GetScreenHeight(), !IsWindowFullscreen(), st.shadowQuality);
    // apply persisted settings to the live game state
    g.set = st;
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
