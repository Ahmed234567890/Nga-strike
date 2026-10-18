#ifndef UI_H
#define UI_H

#include "common.h"
#include "game.h"

void UI_DrawHUD(const Player* player, const Weapon* w, const Weapon* weapons,
                int curWeapon, bool reloading, float reloadT,
                int kills, int score, float gameTime, float adsBlend, bool mp);
void UI_DrawNetHud(const NetState* net, Camera3D cam, float gameTime, int myKills);
void UI_DrawCrosshair(float gap, bool ads, bool scoped);
void UI_DrawPauseMenu(bool* paused, bool* shouldQuit, Game* g);
void UI_DrawDamageVignette(float flash, float lowHp);
void UI_DrawHeadshotBanner(float timer, int streak, int totalHeadshots);

#endif
