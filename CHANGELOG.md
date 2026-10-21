# Changelog

Newest first. **Every meaningful change or event gets an entry here** — features,
fixes, bugs found, tests, design decisions, regressions and process changes.
Documentation rules live in [`AGENTS.md`](AGENTS.md).

## 2026-10-21

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
