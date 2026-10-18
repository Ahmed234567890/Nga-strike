# Nga-Strike Improvement Plan

## Phase 1: Critical Fixes (Immediate) ✅ DONE
- [x] Fix hardcoded absolute sound paths → relative paths with error handling
- [x] Add `SetRandomSeed(time(NULL))` for proper randomness
- [x] Fix AKR sound loading (shot_akr.mp3 not shot.mp3)
- [x] Fix pickup sound (use proper pickup sfx, not jump)
- [x] Rename executable & window title (non-offensive)
- [x] Remove dead weapon switch code (lines 1229-1230)
- [x] Add delta-time safety clamp for physics stability

## Phase 2: Gameplay Logic Fixes ✅ DONE
- [x] Fix enemy accuracy formula (scale to 5% at max range)
- [x] Fix headshot detection (proper head hitbox)
- [x] Fix enemy corner-sticking movement (combined X/Z collision)
- [x] Fix ceiling collision (handle upward velocity)
- [x] Fix player death ammo reset (only current weapon)

## Phase 3: Resource Safety ✅ DONE
- [x] Add error checking for all LoadSound calls
- [x] Add SoundLoaded checks before PlaySound
- [x] Add LoadRenderTexture error check
- [x] Guard UnloadSound calls with validity checks

## Phase 4: Architecture - Split into Modules
- [ ] Create header files: player.h, enemy.h, weapon.h, pickup.h, audio.h, world.h, ui.h, common.h
- [ ] Split main.c into: main.c, player.c, enemy.c, weapon.c, pickup.c, audio.c, world.c, ui.c
- [ ] Move structs to common.h
- [ ] Create proper build.ninja with clean, debug/release configs

## Phase 5: Quality of Life
- [ ] Add config system (sensitivity, volume, FOV, keybinds)
- [ ] Add settings menu
- [ ] Add kill counter / score display
- [ ] Add persistent config file (JSON or INI)
- [ ] Improve sniper scope (actual zoom render)
- [ ] Add footstep sound variation

## Phase 6: Polish
- [ ] Add particle effects (impacts, blood, casings)
- [ ] Add weapon idle/inspect animations
- [ ] Add enemy melee at close range
- [ ] Add bullet tracers / impact decals
- [ ] Pause audio on pause menu

## Missing Assets (need to be added)
- hit.mp3 (for bullet impact on walls)
- enemy_hurt.mp3 (for enemy taking damage)