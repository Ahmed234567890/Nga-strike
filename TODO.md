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

## Phase 13: Deterministic City Maps, AI Weapons & Rendering ✅ DONE
- [x] Preserved `CLASSIC` as the default and added `RANDOM 100` / `RANDOM 256`
      modes (`world` 1/2 in `nga.cfg`); changing `WORLD (NEXT MAP)` only affects
      the next regeneration
- [x] Replaced scatter layout with a seeded road grid: 3×3 or 4×4 site grids,
      sidewalk pads, curbs, lane dashes, perimeter ring, street furniture, lamps,
      barrels and reachable pickups
- [x] Added eight site builders: compound, warehouse, container yard, ruin,
      checkpoint, plaza, market and construction; compact fallback only if a
      site cannot be placed
- [x] Reserved spawn/gate/loot/prop areas, checked site self-overlap, and kept a
      flood-fill connectivity check with bounded retries and a safe open fallback
- [x] Fixed the unreachable central container-yard loot and the site builders
      that were silently never placed: compound roof/body and market
      canopy/stall boxes were touching by floating-point epsilon, and several
      fixed-size props collided on the 100-map (guardhouse/crates, warehouse
      lean-to, checkpoint sandbag, plaza benches). All eight kinds now appear in
      both 120-seed sweeps (100 and 256)
- [x] Added host-owned world mode/seed/epoch sync in welcome and snapshots;
      `NEW MAP` requires a second press, is host/single-player only, resets the
      round and moves late joiners/reconnects onto the live world
- [x] Gave bots three distinct guns (rifle 25 m, SMG 17 m, marksman sniper 52 m)
      with separate damage, accuracy, cadence, burst and sound profiles; added
      visible magazine reloads, marksman lane-holding, obstacle steering and
      no sniper suppression fire
- [x] Added enemy frustum/distance culling and a simple distant model; health
      bars only draw within 42 m; random-map sun shadow casters use raw collision
      boxes while Classic keeps full shadow casting
- [x] Verified: 120 seeds per random mode with zero reachability failures and no
      fallback; all eight site kinds used; runtime single-player remap, live
      host/client remap (`seed 1502055072`, epoch 2), late join and reconnect
      all matched; visual reload/LOD checks passed
- [x] Performance spot check on the development machine (1280×720, HIGH
      shadows, Intel UHD): 256 map ~170 AI / ~170 no-AI FPS, 100 map ~144 AI
      FPS, Classic ~82 FPS; close-range combat ~136–154 FPS

## Phase 14: Layout Verification, Bot Unstuck Navigation & Seed Sharing ✅ DONE
- [x] Added a 64-bit FNV-1a `World_LayoutHash` over collision geometry, spawns,
      barrels, pickups/types, lamps, site kinds and city metadata; log it on
      every generation, `NEW MAP` and remote-map apply
- [x] Host sends `worldHash` in welcome and snapshots; clients compare after
      rebuilding and log/feed `MAP HASH MISMATCH` on divergence, including
      hash-only changes with an unchanged epoch
- [x] Added a 128×128 static walkability grid, 8-way A* with no corner cutting,
      nearest-open snapping and line-of-sight waypoint smoothing
- [x] Bots log `AI STUCK` after 1.25 s of blocked local steering, request a
      path, follow it while they have no firing solution, repath after 1.2 s and
      drop it on combat/cover/death
- [x] Fixed pending world selection: `WORLD (NEXT MAP)` stages
      `Game.nextWorldMode`; only `NEW MAP` changes the active map and saves
      `nga.cfg`. Quitting with a pending selection restores the previous map
- [x] Added **`COPY MAP SEED`** (clipboard) and `--world` / `--seed` startup
      overrides with aliases, range validation and Classic/seed handling
- [x] Moved the 120-seed city/content harness, site sweep, settings checks and
      bot stuck/nav regression into `tests/`; `ninja check` builds and runs all
      four binaries
- [x] Verified: all tests pass, `test_world`/`test_enemy_nav` clean under
      ASan+UBSan; pending selection discard/restart; copied seed
      `1600455712` reproduced hash `6023b67dacd06ba7`; host/client and late
      join/reconnect all matched live remap hash `9d27a1377438f022`; no
      `NEW MAP` or config write occurred before confirmation

## Open Items
- [ ] **G1: Enemies are pinned to the ground** — `enemy.c` forces y ≈ 0.9 every
      frame, so enemies can't stand on platforms/raised geometry (they clip it).
      Needs a design pass plus on-platform visual testing.
- [ ] **Bot navigation still uses static geometry only** — the 128×128 A* grid
      is rasterized from map collision boxes. Barrels, players and other dynamic
      obstacles are not included, and the fallback is only requested after
      local steering fails for 1.25 s. A dynamic/flow-field layer would be the
      next step if bots need to path around movable cover.
- [ ] **In-game text entry for map seeds** — seeds can be copied from the menu
      and supplied with `--seed` at startup, but there is no field to type or
      paste a seed into a running game.
- [ ] **Protocol version negotiation** — the world-hash fields changed the UDP
      layout, so old/new binaries do not interoperate. A version field in the
      handshake could reject mismatched builds instead of dropping packets.
- [ ] **Hash-mismatch recovery** — a client logs and feeds the mismatch but
      keeps its locally generated map; it does not retry generation or block
      play. Decide whether a future protocol should hard-fail the connection.
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
