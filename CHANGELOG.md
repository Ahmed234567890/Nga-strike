# Changelog

Newest first. **Every meaningful change or event gets an entry here** — features,
fixes, bugs found, tests, design decisions, regressions and process changes.
Documentation rules live in [`AGENTS.md`](AGENTS.md).

## 2026-10-25

### Layout verification, bot unstuck navigation, seed sharing and in-repo tests

#### Layout identity and host/client verification

- Added `World_LayoutHash`, a 64-bit FNV-1a checksum over the gameplay-relevant
  layout: mode, seed, playable limit, every collision box center/half-extent,
  all player and enemy spawns, barrels, pickups with their types, street and
  accent lamps, site kinds/count and the city grid metadata. Cosmetic colours
  are deliberately excluded. The hash is logged for every generated map, live
  remap and remote map.
- Added `worldHash` to `PktWelcome` and `PktSnapshot`; `Net_SetWorld` now carries
  it. Clients record the host hash when connecting and compare it after
  generating the host's mode/seed/epoch. A match logs the same hash on both
  sides; a mismatch logs `REMOTE MAP HASH MISMATCH` with both values and feeds
  `MAP HASH MISMATCH` to the client overlay. A hash-only difference also counts
  as a world change, so a client rebuilds even when mode/seed/epoch match.
- This changes the UDP wire format. Old and new binaries are **not compatible**;
  rebuild the host and every client together.

#### Pending world selection and restart fix

- Added `Game.nextWorldMode`. The **`WORLD (NEXT MAP)`** row now only stages a
  selection: it displays `(PENDING)` and leaves the live map and `nga.cfg`
  untouched until **`NEW MAP`** is confirmed. Previously changing the row wrote
  `world` to the config immediately, so a restart without regenerating could
  load a different map from the one that had been on screen.
- **`NEW MAP`** now consumes the pending mode, generates the new seed, saves the
  active mode/seed, advances the epoch and resets the round. A pending selection
  that was never applied is intentionally discarded on restart.
- Added **`COPY MAP SEED`** to the menu (after `NEW MAP`). It copies the current
  random map's numeric seed to the system clipboard and logs
  `MAP SEED <n> copied to clipboard`. Classic has no seed and reports
  `MAP SEED: classic layout has no seed` instead of copying a misleading value.
  In multiplayer the action also feeds `MAP SEED COPIED` / `CLASSIC MAP - NO SEED`.
- Verified the restart rule with a clean temp config: started `random100` seed
  `42` (hash `f647d74a35fa8bf6`), selected `RANDOM 256` which showed
  `(PENDING)`, confirmed `nga.cfg` stayed `world 1` / `worldseed 42` and no
  `NEW MAP` was logged, quit through the menu, then relaunched. The second run
  generated `random100` seed `42` again with the same hash, logged no
  `NEW MAP`, and the menu showed `RANDOM 100` with no pending marker and
  `COPY MAP SEED - 42`.
- Verified seed sharing on a live `random256` seed `1600455712`
  (hash `6023b67dacd06ba7`): `COPY MAP SEED` put `1600455712` on the clipboard,
  and restarting with `--world random256 --seed 1600455712` reproduced the same
  hash.

#### Bot stuck instrumentation and coarse A* fallback

- Added a 128×128 walkability raster to every `WorldMap` (`navCell` is 0.75 m
  on the 100 map and 2 m on the 256 map). It is rasterized directly from
  collision boxes with a 0.38 m body radius, ignores overhead roofs/canopies,
  and is rebuilt with the map; it is never serialized.
- Added `World_NavPath`: 8-way A* with no diagonal corner cutting, nearest-open
  cell snapping, line-of-sight path smoothing and a 96-waypoint cap. The path
  excludes the start and ends at the goal when that point is clear.
- Bots still use local steering and wall-following first. If a bot moves less
  than 22% of its expected step for 1.25 s, it logs `AI STUCK` (throttled to once
  per 4 s), requests an A* path to its last-seen/known goal and follows the
  smoothed waypoints. It repaths after 1.2 s, drops the path when it gains a
  firing solution or enters cover/death, and only overrides the normal chase
  wish while it has no firing solution, preserving combat strafing. A debug
  `AI NAV` line reports the path target and waypoint count.
- Verified with `tests/test_enemy_nav`: a bot facing a dead-end corridor logs
  `AI STUCK: bot 0 ... 1.27s nav=none`, activates the fallback, follows 3
  waypoints and reaches `z=-6.9` behind the wall; an open-field bot moves 1.68 m
  without ever requesting navigation.

#### In-repo regression suite

- Added four test binaries under `tests/` (no `/tmp` dependencies):
  `test_world`, `test_sites`, `test_settings` and `test_enemy_nav`, plus
  `tests/run_tests.sh`. `ninja check` builds and runs all four; `ninja clean`
  removes their binaries and the sentinel. Added `-I.` to the build flags so the
  tests compile from the repo layout. Added `.gitignore` entries for the debug
  binary, test binaries, sentinel and temporary test config.
- `test_world`: Classic hash is fixed at `b15336a866fa6517` and ignores the
  random seed; six fixed random hashes are checked (`random100` seeds 1, 42,
  123456789 and `random256` seeds 1, 42, 123456789); a 120-city sweep (60 per
  mode) checks deterministic regeneration, content bounds, pickup types,
  nav-grid density, BFS reachability of every spawn/loot/barrel/lamp, A* path
  waypoints and endpoints; seed 0 normalizes to seed 1; editing a collision box,
  pickup or player spawn changes the hash.
- `test_sites`: 80 seeds per mode (2000 sites total), every block receives a
  site, all kinds 0..7 appear, each site owns at least 7 low collision boxes,
  and site kinds are deterministic on regeneration.
- `test_settings`: defaults, float `sens`/`volume` and high-seed round-trip,
  missing/empty config, invalid-value sanitising, unknown keys and sparse files.
- `test_enemy_nav`: the dead-end and open-field cases above; it captures and
  asserts the `AI STUCK` log.
- `ninja check` passes all four tests. `test_world` and `test_enemy_nav` also
  pass under ASan+UBSan (`-fsanitize=address,undefined`) with no sanitizer
  errors; leak detection was left off because raylib keeps process-global state.
  Exact results: 120 deterministic city seeds OK; 720 small + 1280 large sites,
  observed minimum 7 boxes/site (the test asserts at least 2); settings
  round-trip OK; 223 nav frames / 3 waypoints in the dead end and 1.68 m of
  open-field movement without nav.

#### Network and playtest verification

- Host `--host 7788 --no-upnp --world random100 --seed 42` logged
  `layout hash=f647d74a35fa8bf6`; a client started on Classic, connected as id 2
  with `hash=f647d74a35fa8bf6`, generated the host map and logged
  `REMOTE MAP ... hash=f647d74a35fa8bf6`.
- Live host remap to `random256` seed `1194348036`, epoch 2 generated
  `layout hash=9d27a1377438f022`; the connected client applied the same epoch,
  seed and hash without reconnecting.
- A late-join client started on local `random100` seed `1`
  (hash `251345f5a3e7cbb1`), connected as id 3 and immediately applied the live
  `9d27a1377438f022` map. After the client was killed and timed out, it
  relaunched, reconnected as id 3 and again applied the same live map/hash.
- Earlier playtest: `--world random100 --seed 42` produced
  `f647d74a35fa8bf6`; `NEW MAP` from a pending `RANDOM 256` produced seed
  `1600455712`, epoch 2, hash `6023b67dacd06ba7`. All 16 menu rows fit at
  1280×720. `nga.cfg` was kept local and was not committed.

#### Test-process notes

- Synthetic XTEST key events were unreliable under Xwayland on this desktop:
  the first pending-restart attempt did not open the menu even with input focus
  on the game window. The test harness switched to direct X `SendEvent`
  key events, which reproduced normal menu navigation reliably.
- Multiple playtest windows were stacked at the same screen coordinates, so
  synthetic clicks/keys could hit the wrong instance. The host/client instances
  were stopped before the final restart test. The first host remap attempt
  clicked the wrong menu rows and did nothing; the corrected local coordinates
  (window-relative, not root-relative) produced the verified remap.
- `pgrep` briefly matched the shell wrapper instead of the game process; window
  lookup by `_NET_WM_PID` is the reliable way to target a specific instance.

## 2026-10-21

### Deterministic city maps, distinct AI guns, world sync and render culling

#### City generation (`CLASSIC` remains the default)

- Replaced the random scatter layout with a seeded, intentional road grid.
  `WORLD_RANDOM` (`world 1`, 100×100) now uses a 3×3 block grid and
  `WORLD_RANDOM_256` (`world 2`, 256×256) a 4×4 grid. Block centers, road
  widths, sidewalks, curbs, lane paint, perimeter roads and world bounds are
  defined per mode instead of being sprinkled at random.
- Every block gets a designed site chosen from eight builders: compound,
  warehouse, container yard, ruin, checkpoint, plaza, market and construction.
  Sites have walls and gate openings, interior cover and loot. A compact site
  exists only as a last-resort fallback.
- Added safe reserved zones around player spawns, site gates, loot, barrels and
  lamps; street furniture and decorative props are placed with collision and
  clearance checks. The old hardcoded arena pickups/barrels/lights now come from
  world metadata for both Classic and random maps.
- Generation is deterministic from `(mode, seed)`, validates site self-overlap,
  checks that all spawns/loot/barrels are reachable with a flood fill, retries
  other seeds/layouts on failure, and falls back to a bounded open arena with
  cover if a valid city cannot be produced. The fallback keeps the existing
  pickups/barrels reachable instead of leaving content inside deleted geometry.
- Fixed the container-yard centre loot dead zone by leaving the loading square
  open, so the centre pickup no longer sits inside a closed container ring.
- Added compact guardhouses, sidewalk/curb hierarchy, lamps, parked/abandoned
  cars, barriers, crates, grass/pebbles/rocks, and a boundary wall. All random
  prop geometry is integrated into the normal collision and shadow passes.

#### Site-coverage bug found during the 120-seed sweep

- Kinds `0` (compound) and `6` (market) were **never placed**. Their boxes were
  rejected by the self-overlap check because geometry touched by a floating
  point epsilon: the compound guardhouse roof sat exactly on the body top, and
  market canopies sat exactly on the stall back panels.
- The 100×100 layout exposed further fixed-size collisions: guardhouse versus
  crates/low wall, warehouse lean-to versus crates/wall, checkpoint sandbag
  versus its low wall, and plaza benches versus the fountain base.
- Fixed all of them with small, visually invisible clearances/repositioning.
  Site plans now pass 200 RNG seeds for both map sizes, and both site sweeps use
  all eight kinds with no compact fallback (kind 8).

#### AI weapons, reloads and city movement

- Enemy guns now have real profiles: assault rifle (25 m, 4.0 base damage),
  SMG (17 m, 2.7, fast 4–7 round bursts, pitched pistol crack), and marksman
  sniper (52 m, 16.0, slow single shots, sniper sound, no suppression fire).
  Aim time, cooldown, burst count and accuracy falloff differ by gun.
- Marksmen keep a preferred ~27 m lane instead of closing to SMG range. Bots
  lead strafing targets, become more accurate with sustained visibility and
  lose accuracy when moving, airborne, flinching or suppressed.
- Added magazine ammo, reload timers and a visible reload animation (gun tilt,
  magazine removal/insertion). The host simulates reloads and clients receive
  the same phase/mag values in enemy snapshots.
- Added obstacle steering/wall-follow probes and a stuck-timer side commitment
  so bots slide around city corners instead of pushing straight into walls.
  This is steering, not full pathfinding; see `TODO.md`.
- Fixed enemy gun/hand alignment by removing an extra weapon-space translation
  and applying recoil (`gunKick`) to the aim pose.
- Added enemy frustum/distance culling and a simpler distant model beyond 26 m;
  health bars are only drawn within 42 m. The full model remains for close
  fights and reload inspection.

#### World identity, live remaps and networking

- Added `worldMode`, `worldSeed` and `worldEpoch` to the host welcome and
  snapshots. Clients rebuild the host's current city (including on first
  connect) and apply a live remap when the epoch changes, without reconnecting.
- Added `Game_NewMap` host/single-player lifecycle: generate, increment epoch,
  save settings, rebake lamps and reset the round. **`NEW MAP (REGENERATE)`**
  requires a second confirmation within three seconds and is disabled on
  clients (`HOST ONLY`).
- Late joiners and reconnecting clients receive the live map and current epoch;
  their local `world`/`worldseed` settings are ignored while connected.
- Clients keep their locally generated enemy proxies hidden until host
  snapshots arrive, including after a remote map rebuild, so host-owned enemies
  never ghost at the wrong spawn points.
- The HUD hint is now `1/2/3 WEAPON`, and the menu row is
  `WORLD (NEXT MAP)` so changing it does not imply an immediate rebuild.
- Fixed stale map limits on player movement and client barrels/pickups being
  initialized from Classic coordinates.

#### Rendering and performance

- Random maps now use a lightweight sun-caster pass that draws the raw collision
  boxes, instead of re-running the full lit/decorated world into the shadow map.
  Classic retains its original full casters, so its look is unchanged.
- Added map-aware fog uniforms, city ground rendering that is independent of
  shadow quality, and a cheaper enemy LOD for long sightlines.
- Development-machine observations (1280×720, HIGH shadows, uncapped, Intel
  UHD): 256 map ~171 FPS with AI and ~170 without AI at spawn, 100 map ~144 FPS,
  Classic ~82 FPS; close-range combat ~136–154 FPS. These are observations, not
  guaranteed results on other hardware. Multiple simultaneous clients reduce
  FPS because they share the GPU.

#### Verification

- Release build: `ninja` (no warnings).
- Settings unit test: `world`/`worldseed` round-trip with float `sens`/`volume`
  passed; invalid `world 9`/`worldseed 0` sanitized to Classic/seed 1.
- Deterministic harness (120 seeds per mode): `FAILS 0`, no fallback warnings;
  100 map 9 sites / 8 pickups / 6 barrels / 4 lamps, boxes 109–135; 256 map
  16 sites / 20 pickups / 6 barrels / 4 lamps, boxes 209–246. Every seed's
  site count, site kind and enemy spawns were checked. Site-kind totals across
  both sweeps: `0:555 1:342 2:415 3:359 4:281 5:445 6:308 7:295`; no kind 8.
- Site builder self-overlap sweep: all kinds 0–8, 200 RNG seeds at both 23 m
  and 9.2 m site half-extents, zero failures.
- Single-player: menu opened, navigated to `NEW MAP`, first Enter armed,
  second Enter generated `mode=1 seed=85346002 epoch=2 (110 boxes)`, and
  Escape closed the menu and resumed the new layout; the window stayed
  1280×720 throughout.
- Live network test with host `--host 7777 --no-upnp` and client
  `--join 127.0.0.1:7777`: client started on a different local map and logged
  `REMOTE MAP mode=2 seed=777 epoch=1 (231 boxes)`; host then generated
  `NEW MAP mode=2 seed=1502055072 epoch=2 (223 boxes)` and the client logged
  the same seed/epoch/box count. Late join id 3 and reconnect id 4 both
  received `REMOTE MAP mode=2 seed=1502055072 epoch=2`. No peer timeouts.
- Visual/reload test at close range rendered ready, reload 30%, 55% and 85%
  poses for rifle, SMG and sniper; magazine and hand alignment remained
  attached. Gameplay captures confirmed enemy health bars and close-range AI.
- `nga.cfg` was backed up and restored after every automated run; it and the
  untracked `nga_strike_debug` binary are not part of this change.

#### Test-process notes

- `XGetImage` captures of overlapped game windows produced ghosted HUD text;
  unmapping/raising the target window produced clean captures. This is a
  screenshot-harness artifact, not a game HUD regression.
- One early automated remap run resized the host window to 1600×900 while the
  client was unmapped. It did not reproduce in two focused keyboard-only
  diagnostics or the final full network run (all stayed 1280×720); likely
  synthetic focus/pointer input. Left as an unconfirmed test-harness artifact.

#### Files

`world.c`, `world.h`, `enemy.c`, `enemy.h`, `game.c`, `game.h`, `net.c`,
`net.h`, `player.c`, `player.h`, `pickup.c`, `pickup.h`, `shadows.c`,
`shadows.h`, `settings.c`, `settings.h`, `ui.c`, `common.h`, `README.md`,
`TODO.md`, `CHANGELOG.md`.

### Optional bot infighting — `BOTS FIGHT EACH OTHER` — `40117b6`

- Added **BOTS FIGHT EACH OTHER** to the settings menu, persisted as
  `infighting` in `nga.cfg` and default OFF so existing matches behave exactly
  as before.
- When ON, host/single-player AI adds every living bot as a valid target (one
  fake `Player` stub per bot). A bot skips its own stub; target selection still
  picks the nearest visible opponent and uses the real enemy eye position for
  line-of-sight checks.
- Stub health loss is converted back into `Enemy_ApplyDamage` after
  `Enemy_UpdateAll`, so health, death, corpse/respawn timers and enemy snapshots
  stay authoritative. No network protocol change was needed.
- Bot-on-bot damage spawns blood; kills play the death sound, add a blood decal
  and log `AI infighting: bot N was killed by another bot`. Bot kills award no
  player score or kill-feed credit.
- Host/single-player control only: clients see `HOST` and changes are ignored,
  matching `AI ENEMIES`. The host can toggle infighting live.
- Design note: bots still pick the nearest visible target, so a nearby player can
  remain the focus; the setting makes other bots valid opponents rather than
  forcing bots to ignore players.
- Verification:
  - Settings round-trip including `infighting` passed.
  - Single-player idle 75 s: 246 bot-vs-bot damage events and 7 bot kills.
  - Host/client 40 s smoke test: two players connected, the host logged 5
    infighting kills, and neither side reported a timeout or silent-host error.
  - Single-player OFF 35 s: zero infighting logs.
  - Menu: single-player toggled off/on and persisted; the client-only run left
    the `HOST` row unchanged.
- Files: `settings.c`, `settings.h`, `ui.c`, `enemy.c`, `enemy.h`, `game.c`,
  `README.md`, `TODO.md`.

### Multiplayer live settings menu + settings persistence — `9a55b20`

- `Esc` now opens the settings overlay while hosting or joining. The match keeps
  running: the net pump, host simulation, AI and snapshots are never paused.
- While the overlay is open, local movement, look, fire, reload, jump and
  weapon-switch input is suppressed. The player stands idle and can still be
  shot — this is an online menu, not a pause.
- The overlay title is `SETTINGS (LIVE)` and the multiplayer HUD hint says
  `ESC MENU (LIVE)`.
- `AI ENEMIES` is host-only in multiplayer: clients see `HOST`, and attempts to
  change it are ignored. The host can toggle AI off/on live.
- Fixed `sens` and `volume` never reloading from `nga.cfg`: the old integer-first
  `sscanf` parsed the leading `0` of decimal values and skipped the float branch.
  Loading now parses a key/value pair and converts with `atoi`/`atof`.
- Verification:
  - Single-player: `Esc` logs `menu opened/closed (single-player: world paused)`.
  - Multiplayer: host and client menus both opened and stayed live for 12 s with
    no host peer timeout and no client “host silent” warning.
  - Host AI toggled off/on live (`AI enemies: off/on`) and persisted `ai`.
  - Client AI row logged no change and `ai` stayed `1`.
  - Host menu volume changed `0.500 → 0.550` and survived `nga.cfg` save/load.
  - Full int+float settings round-trip unit test passed.
  - Two-instance host/client connection smoke test passed.
- Files: `game.c`, `player.c`, `player.h`, `ui.c`, `settings.c`, `README.md`,
  `TODO.md`.

### AKR single-shot sound — `9ebe3c1`

- Replaced the 2.14 s / 14-shot AKR burst recording with a 94 ms single shot,
  matching the AKR's 105 ms fire interval. One shot per trigger pull.

### AI enemies toggle — `f665919`

- Added the persisted `AI ENEMIES` setting (`ai` in `nga.cfg`): off despawns all
  bots, clears corpses and stops spawning; on restores them on staggered
  respawn timers.
- Added `--no-ai` for bot-free host/client sessions. The host remains the AI
  authority; clients mirror host snapshots.

### Documentation policy

- Per the user's request, added this changelog and [`AGENTS.md`](AGENTS.md):
  going forward every change and event is documented, including tests, bugs and
  process decisions.
