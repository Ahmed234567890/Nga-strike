#ifndef AUDIO_H
#define AUDIO_H

#include "common.h"

typedef struct {
    Sound shot;
    Sound shotAKR;
    Sound shotSniper;
    Sound reload;
    Sound footstep;
    Sound jump;
    Sound hit;        // may be missing on disk -> fallback at play time
    Sound enemyHurt;  // may be missing on disk -> fallback at play time
    Sound enemyDie;
    Sound playerHurt;
    Sound pickup;     // may alias missing hit -> fallback at play time
    Sound shotSounds[WEAPON_COUNT];
    float masterVolume;
} Audio;

void Audio_Init(Audio* audio);
void Audio_Unload(Audio* audio);
void Audio_SetVolumes(Audio* audio);
void Audio_ApplyMaster(Audio* audio);

bool SoundLoaded(Sound s);
Sound LoadSoundSafe(const char* filename);
void UnloadSoundSafe(Sound* s);
void PlaySoundSafe(Sound s);
void PlaySoundPitched(Sound s, float pitch);

// Fallback-safe one-shots (never double-free, never crash on missing files)
void Audio_PlayShot(Audio* a, int weaponIdx, float pitch);
void Audio_PlayHit(Audio* a, float pitch);
void Audio_PlayHeadshot(Audio* a);
void Audio_PlayHelmetPing(Audio* a);
void Audio_PlayReloadClick(Audio* a, float pitch);
void Audio_PlayExplosion(Audio* a);
void Audio_PlayEnemyHurt(Audio* a, float pitch);
void Audio_PlayPickup(Audio* a, float pitch);

#endif // AUDIO_H
