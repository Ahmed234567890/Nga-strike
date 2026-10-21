# Changelog

Newest first. **Every meaningful change or event gets an entry here** — features,
fixes, bugs found, tests, design decisions, regressions and process changes.
Documentation rules live in [`AGENTS.md`](AGENTS.md).

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
