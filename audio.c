#include "audio.h"
#include <stdio.h>
#include <string.h>

Sound LoadSoundSafe(const char* filename) {
    char path[512];
    const char* tries[3];
    char b1[512], b2[512];
    snprintf(b1, sizeof(b1), "./%s", filename);
    snprintf(b2, sizeof(b2), "./assets/%s", filename);
    tries[0] = b1;
    tries[1] = filename;
    tries[2] = b2;
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s", tries[i]);
        if (!FileExists(path)) continue;
        Sound s = LoadSound(path);
        if (s.frameCount > 0) return s;
    }
    TraceLog(LOG_WARNING, "Missing sound: %s (using silent fallback)", filename);
    return (Sound){ 0 };
}

void UnloadSoundSafe(Sound* s) {
    if (s->frameCount > 0 && s->stream.buffer != NULL) {
        UnloadSound(*s);
        s->frameCount = 0;
        s->stream.buffer = NULL;
    }
}

bool SoundLoaded(Sound s) {
    return s.frameCount > 0 && s.stream.buffer != NULL;
}

void PlaySoundSafe(Sound s) {
    if (SoundLoaded(s) && IsAudioDeviceReady()) PlaySound(s);
}

void PlaySoundPitched(Sound s, float pitch) {
    if (!SoundLoaded(s) || !IsAudioDeviceReady()) return;
    SetSoundPitch(s, pitch);
    PlaySound(s);
}

void Audio_Init(Audio* a) {
    memset(a, 0, sizeof(*a));
    a->masterVolume = 1.0f;
    a->shot       = LoadSoundSafe("shot.mp3");
    a->shotAKR    = LoadSoundSafe("shot_akr.mp3");
    a->shotSniper = LoadSoundSafe("shot_sniper.mp3");
    a->reload     = LoadSoundSafe("reload.mp3");
    a->footstep   = LoadSoundSafe("footstep.mp3");
    a->jump       = LoadSoundSafe("jump.mp3");
    a->hit        = LoadSoundSafe("hit.mp3");
    a->enemyHurt  = LoadSoundSafe("enemy_hurt.mp3");
    a->enemyDie   = LoadSoundSafe("enemy_die.mp3");
    a->playerHurt = LoadSoundSafe("player_hurt.mp3");
    // pickup: prefer dedicated file if present, else silent (fallback plays reload)
    a->pickup     = LoadSoundSafe("pickup.mp3");

    a->shotSounds[0] = a->shot;
    a->shotSounds[1] = SoundLoaded(a->shotAKR) ? a->shotAKR : a->shot;
    a->shotSounds[2] = SoundLoaded(a->shotSniper) ? a->shotSniper : a->shot;
    Audio_SetVolumes(a);
}

void Audio_SetVolumes(Audio* a) {
    if (SoundLoaded(a->shot))       SetSoundVolume(a->shot, 0.75f);
    if (SoundLoaded(a->shotAKR))    SetSoundVolume(a->shotAKR, 0.75f);
    if (SoundLoaded(a->shotSniper)) SetSoundVolume(a->shotSniper, 0.85f);
    if (SoundLoaded(a->reload))     SetSoundVolume(a->reload, 0.70f);
    if (SoundLoaded(a->footstep))   SetSoundVolume(a->footstep, 0.25f);
    if (SoundLoaded(a->jump))       SetSoundVolume(a->jump, 0.45f);
    if (SoundLoaded(a->hit))        SetSoundVolume(a->hit, 0.45f);
    if (SoundLoaded(a->enemyHurt))  SetSoundVolume(a->enemyHurt, 0.55f);
    if (SoundLoaded(a->enemyDie))   SetSoundVolume(a->enemyDie, 0.65f);
    if (SoundLoaded(a->playerHurt)) SetSoundVolume(a->playerHurt, 0.70f);
    if (SoundLoaded(a->pickup))     SetSoundVolume(a->pickup, 0.55f);
}

void Audio_ApplyMaster(Audio* a) {
    SetMasterVolume(ClampF(a->masterVolume, 0.0f, 1.0f));
}

void Audio_PlayShot(Audio* a, int idx, float pitch) {
    if (idx < 0) idx = 0;
    if (idx >= WEAPON_COUNT) idx = 0;
    Sound s = a->shotSounds[idx];
    if (!SoundLoaded(s)) s = a->shot;
    PlaySoundPitched(s, pitch);
}

void Audio_PlayHit(Audio* a, float pitch) {
    if (SoundLoaded(a->hit)) { PlaySoundPitched(a->hit, pitch); return; }
    // fallback: short quiet reload click
    if (SoundLoaded(a->reload)) PlaySoundPitched(a->reload, pitch * 1.4f);
}

void Audio_PlayHeadshot(Audio* a) {
    // crisp high ding: layer hit + pitched hurt/die
    if (SoundLoaded(a->hit)) PlaySoundPitched(a->hit, 1.7f);
    else if (SoundLoaded(a->reload)) PlaySoundPitched(a->reload, 2.0f);
    if (SoundLoaded(a->enemyDie)) PlaySoundPitched(a->enemyDie, 1.35f);
    else if (SoundLoaded(a->enemyHurt)) PlaySoundPitched(a->enemyHurt, 1.5f);
}

void Audio_PlayHelmetPing(Audio* a) {
    if (SoundLoaded(a->hit)) PlaySoundPitched(a->hit, 2.2f);
    else if (SoundLoaded(a->reload)) PlaySoundPitched(a->reload, 2.4f);
    if (SoundLoaded(a->shot)) PlaySoundPitched(a->shot, 1.8f);
}

void Audio_PlayReloadClick(Audio* a, float pitch) {
    if (SoundLoaded(a->reload)) { PlaySoundPitched(a->reload, pitch); return; }
    if (SoundLoaded(a->hit)) PlaySoundPitched(a->hit, pitch * 0.8f);
}

void Audio_PlayExplosion(Audio* a) {
    if (SoundLoaded(a->shotSniper)) { PlaySoundPitched(a->shotSniper, 0.55f); return; }
    if (SoundLoaded(a->shotAKR)) PlaySoundPitched(a->shotAKR, 0.6f);
}

void Audio_PlayEnemyHurt(Audio* a, float pitch) {
    if (SoundLoaded(a->enemyHurt)) { PlaySoundPitched(a->enemyHurt, pitch); return; }
    if (SoundLoaded(a->enemyDie)) { PlaySoundPitched(a->enemyDie, pitch * 1.25f); return; }
    if (SoundLoaded(a->hit)) { PlaySoundPitched(a->hit, pitch); }
}

void Audio_PlayPickup(Audio* a, float pitch) {
    if (SoundLoaded(a->pickup)) { PlaySoundPitched(a->pickup, pitch); return; }
    if (SoundLoaded(a->reload)) { PlaySoundPitched(a->reload, pitch); return; }
    if (SoundLoaded(a->jump)) { PlaySoundPitched(a->jump, pitch); }
}

void Audio_Unload(Audio* a) {
    UnloadSoundSafe(&a->shot);
    UnloadSoundSafe(&a->shotAKR);
    UnloadSoundSafe(&a->shotSniper);
    UnloadSoundSafe(&a->reload);
    UnloadSoundSafe(&a->footstep);
    UnloadSoundSafe(&a->jump);
    UnloadSoundSafe(&a->hit);
    UnloadSoundSafe(&a->enemyHurt);
    UnloadSoundSafe(&a->enemyDie);
    UnloadSoundSafe(&a->playerHurt);
    UnloadSoundSafe(&a->pickup);
    // shotSounds are aliases/views only; do not unload.
}
