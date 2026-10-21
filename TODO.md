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

## Phase 4: Architecture - Split into Modules ✅ DONE
- [x] Create header files: player.h, enemy.h, weapon.h, pickup.h, audio.h, world.h, ui.h, common.h
- [x] Split main.c into: main.c, player.c, enemy.c, weapon.c, pickup.c, audio.c, world.c, ui.c
      (later grew: fx.c, damage_fx.c, shadows.c, settings.c, net.c)
- [x] Move structs to common.h
- [x] Create proper build.ninja with clean, debug/release configs

## Phase 5: Quality of Life ✅ DONE
- [x] Add config system (sensitivity, volume, FOV, resolution, shadows, lights, fog)
      — keybind remapping still not configurable (open item below)
- [x] Add settings menu
- [x] Add kill counter / score display
- [x] Add persistent config file (nga.cfg, INI-style, sanitized on load)
- [x] Improve sniper scope (actual zoom render + scope overlay)
- [x] Add footstep sound variation (random pitch, quieter while sprinting)

## Phase 6: Polish ✅ DONE
- [x] Add particle effects (sparks, blood, head blood, smoke, muzzle smoke,
      casings, pickup bursts, helmet pop, explosions, dust, dirt kicks)
- [x] Add weapon idle/inspect animations
- [x] Add enemy melee at close range (with wind-up telegraph)
- [x] Add bullet tracers / impact decals
- [x] Pause audio on pause menu (duck master volume while paused)

## Phase 7: LAN Multiplayer ✅ DONE
- [x] Host/join CLI: `--host [port]`, `--join IP[:port]`, `--name NAME`; no flags = unchanged SP
- [x] UDP star topology, host as world authority, up to 4 players, packed little-endian wire format
- [x] Host-authoritative enemy AI streamed to clients with interpolation and hit validation
- [x] PvP friendly fire with kill feed, scoreboard, name tags and per-player colors
- [x] Death/respawn flow (3 s timer, spawn shield, unoccupied spawn pick)
- [x] Pickups, barrels, damage and health synced across the star
- [x] Reconnect handling (host loss → stall detect → handshake reset → auto-rejoin;
      server-full retries instead of latching)

## Phase 8: Multiplayer & Gameplay Bug Audit ✅ DONE
- [x] Per-enemy target selection: host enemies fight the local player and every
      remote peer, not just one globally nearest player
- [x] Dropped peers no longer linger as ghosts (host relays synthetic BYE that
      passes clients' stale-seq check)
- [x] Client kill scoring matches the host exactly (elite +100, headshot 150 + 25×streak)
- [x] Client-side aim/windup telegraphs restored from snapshot edge flags
- [x] Pickup consume requests proximity-validated on the host
- [x] Melee shove mirrored client-side from the damage event
- [x] Remote walk-anim speed divisor fixed; respawn avoids occupied spawn points
- [x] LAN hardening: damage clamps + NaN/isfinite rejection, host address validation,
      bad `--join` warns and falls back to single-player
- [x] Cleanups: BARREL_RESPAWN_TIME constant, MP-aware HUD hint, dead code removal

## Phase 9: Internet Hosting (UPnP) ✅ DONE
- [x] `upnp.c/h` (miniupnpc): maps only the game's UDP port while hosting;
      2 h lease refreshed hourly; removed on clean exit; `--no-upnp` opt-out;
      CGNAT / double-NAT detection with a clear warning; logs a share-ready
      `--join` line with the router's public address
- [x] Packet source hardening: every peer-originated datagram must come from
      the address the peer joined from (checked before seq/liveness
      bookkeeping) — spoofed P_BYE can no longer kick a player, forged damage/
      kill/noise events are rejected, seq windows can't be poisoned and ghost
      peers can't be kept alive from elsewhere
- [x] P_NOISE finiteness check, enemy-hit zone range check, relay byte clamps
      (the host never forwards trailing bytes appended by a sender)
- [x] Verified: MP regression unchanged; spoof/fuzz suite (garbage, truncated,
      oversized, NaN, spoofed-from floods ~1000 datagrams) clean under
      ASan+UBSan with the host still joinable afterwards; mapping add/verify/
      remove exercised on a real router

## Phase 10: AI Toggle ✅ DONE
- [x] Pause-menu **AI ENEMIES** setting (persisted as `ai` in `nga.cfg`):
      off despawns every bot, clears corpses and stops all spawning; on brings
      them back on staggered respawn timers
- [x] `--no-ai` launch flag for hosts/clients (pure PvP / empty arena); host is
      the AI authority, clients mirror the snapshot stream

## Phase 11: Live Multiplayer Menu ✅ DONE
- [x] `Esc` opens the settings overlay in host/client sessions without pausing:
      the net pump, host AI simulation and snapshots keep running; local
      movement/look/fire/reload/weapon-switch input is suppressed while it is
      open, so the player stands idle (and vulnerable)
- [x] `AI ENEMIES` is host-only on clients (row shows `HOST`, changes ignored)
- [x] Fixed `sens`/`volume` never loading from `nga.cfg` (the integer-first
      `sscanf` swallowed decimal values); full settings round-trip tested

## Phase 12: Bot Infighting ✅ DONE
- [x] Added the persisted **BOTS FIGHT EACH OTHER** setting (`infighting` in
      `nga.cfg`), default off so existing matches behave exactly as before
- [x] Host/single-player authority: the host AI adds one target stub per living
      bot, skips the shooter's own stub, and converts stub health loss into real
      `Enemy_ApplyDamage` after the AI update; clients see health/death/respawn
      through existing enemy snapshots (no protocol change)
- [x] Added blood, death sound, blood decal and a log line on bot-on-bot kills;
      bot kills award no player score or kill-feed credit
- [x] Clients show `HOST` for this row and cannot change it, matching the existing
      AI row; the host can toggle it live
- [x] Verified: 75 s single-player run with 246 bot-vs-bot damage events and
      7 bot kills; a host/client smoke test logged 5 host infighting kills
      with no timeouts; OFF run produced zero infighting logs; settings
      round-trip and client `HOST` guard both passed

## Open Items
- [ ] **G1: Enemies are pinned to the ground** — `enemy.c` forces y ≈ 0.9 every
      frame, so enemies can't stand on platforms/raised geometry (they clip it).
      Needs a design pass plus on-platform visual testing.
- [ ] **Custom keybinds** (leftover from Phase 5) — controls are fixed in code.
- [ ] **Optional join password for internet hosting** (deferred from Phase 9) —
      a shared secret in the HELLO, rejected on mismatch; empty = open like today.
- [ ] _(Accepted risk)_ Relaying P_DAMAGE / P_KILL reuses the sender's seq number;
      a rare event can be dropped or duplicated. Impact is low, left as-is.

## Missing Assets (optional — silent fallback until added)
- hit.mp3 (bullet impact on walls)
- enemy_hurt.mp3 (enemy taking damage)
- pickup.mp3 (pickup sound)

The game logs `Missing sound: ... (using silent fallback)` and keeps going.
