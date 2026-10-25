# Nga-Strike

A fast-paced arena first-person shooter built from scratch in C with [raylib](https://www.raylib.com/).

Play solo against AI enemies, or jump into a LAN match with up to 4 players — co-op and PvP at the same time.

## Features

- **3 weapons** — semi-auto Pistol, automatic AKR, and a hard-hitting scoped Sniper, each with its own damage, spread, recoil, ADS behavior and reloads.
- **Deterministic city maps** — a classic arena plus seeded **100×100** and **256×256** road-grid cities with blocks, sidewalks, lane markings, eight site archetypes, safe spawns, reachable loot and a playable fallback layout.
- **Verified, shareable layouts** — every random city has a numeric seed and a 64-bit layout hash. Copy the seed from the menu or launch with `--world` / `--seed`; hosts send the hash so clients can verify they rebuilt the same city.
- **Enemy AI with personality** — patrol / chase / strafe / cover / flee behavior, three combat roles (assault, marksman, rusher), elite variants, melee charges with a visible wind-up, and helmets you can shoot off. Each role carries a distinct rifle, SMG or marksman sniper with its own range, cadence, sound and visible magazine reload. Bots steer and wall-follow around cover, and request a coarse A* path when local steering gets stuck.
- **Proper hit zones** — head, torso, leg, and helmet hitboxes with damage numbers, hitmarkers, and headshot streak bonuses.
- **Explosive barrels** — shoot them to clear a room. They hurt you too.
- **Pickups** — health and ammo crates that respawn on a timer. Random cities scatter them through sites and around the road network, and generation checks that every one is reachable.
- **Movement** — sprint, jump, aim-down-sights with per-weapon FOV and sensitivity, and a spawn shield after respawning.
- **Rendering** — dynamic shadow maps (off / low / high / ultra), colored point lights, distance fog, particles, tracers, bullet decals and animated first-person viewmodels. Bots outside the view frustum are culled and distant bots use a cheaper model; random-map sun shadows use a lightweight caster pass while Classic keeps the full lit-shadow pass.
- **HUD** — health, kills, score, timer, reload/ammo display, hitmarkers, headshot banners, and directional damage indicators.

## Maps

Open **`WORLD (NEXT MAP)`** in the pause menu to choose the layout that the
next **`NEW MAP (REGENERATE)`** will build:

| Mode | Layout | Blocks / sites | Pickups | Perimeter |
| --- | --- | --- | --- | --- |
| **CLASSIC** (default) | Original open arena | — | 8 | — |
| **RANDOM 100** | 100×100 city grid | 3×3 / 9 designed sites | 8 target | Road ring at the edge |
| **RANDOM 256** | 256×256 city grid | 4×4 / 16 designed sites | 20 target | Road ring at the edge |

Each random city is built from a road grid with raised sidewalk pads, curbs,
lane dashes, street lamps and furniture, plus eight recurring site types:
walled compounds, warehouses, container yards, ruins, checkpoints, plazas,
markets and construction sites. Spawn points, site gates, pickups, barrels and
decorative props are reserved and checked against the layout so they do not
spawn inside walls. A flood-fill connectivity check verifies that all spawns,
loot and barrels can be reached; if a layout cannot be made safe, generation
retries and ultimately falls back to a bounded open arena with cover.

Generation is deterministic from the map mode and seed. The host sends the
seed, world epoch and a 64-bit layout hash with the welcome and snapshots, so
every connecting client builds the same city and can verify it. A matching hash
is logged on both sides; a mismatch logs `REMOTE MAP HASH MISMATCH` with the
host and local values and shows `MAP HASH MISMATCH` in the client feed. The
client keeps its locally generated layout after a mismatch, so run identical
builds on the host and every client.

Only the host or a single-player game can run **NEW MAP**; it asks for a second
press within three seconds, then resets scores, health, enemies, barrels and
pickups for everyone. Clients follow the host's new world, including players who
join late or reconnect. `world` and `worldseed` in `nga.cfg` record the active
mode and seed; relaunching with those values rebuilds the same deterministic
city.

**`WORLD (NEXT MAP)`** only stages a selection. It shows `(PENDING)` when it
differs from the live map and does not touch `nga.cfg` or the current city until
**NEW MAP** is confirmed. Quitting with a pending selection discards it; the next
launch restores the active map. **`COPY MAP SEED`** copies the live random map's
numeric seed to the clipboard (Classic has no seed), and the seed can be shared
with the startup overrides below.

### Sharing a map seed

```sh
./nga_strike --world random100 --seed 42
./nga_strike --world random256 --seed 1600455712
```

`--world` accepts `classic` / `0`, `random100` / `random` / `100` / `1`, and
`random256` / `256` / `2`. `--seed` accepts `1`–`4294967295`; Classic ignores
the seed. Invalid values log a warning and keep `nga.cfg` unchanged. The
overrides work in single-player, host and client launches. The host's map is
always authoritative; a connecting client rebuilds the host's mode/seed/epoch
(and verifies the hash) regardless of its own `--world` / `--seed` values. The
overrides apply to the startup map and do not rewrite `nga.cfg` on their own;
the next settings save (for example, changing the volume) records the active
mode/seed.

The layout hash covers collision geometry, player/enemy spawns, barrels,
pickups and their types, lamps, site kinds and city-grid metadata. It is meant
to catch host/client generation differences, not to authenticate clients.

## Bot combat

All bots use the same movement and hitbox rules, but their guns are not
cosmetic:

| Role / gun | Range | Behaviour |
| --- | --- | --- |
| Assault / rifle | 25 m | 2–4 round bursts, medium cadence and damage |
| Rusher / SMG | 17 m | 4–7 round close-range bursts, faster and weaker hits |
| Marksman / sniper | 52 m | Single heavy shots, long aim time, prefers about 27 m and holds open lanes |

Bots lose accuracy with distance, lead strafing players, get
more accurate while they keep a target in view, and lose accuracy when moving,
airborne, flinching or suppressed. Every gun has a distinct sound. Reloading
is visible: the weapon tilts down, the magazine drops out and is reinserted,
then the gun comes back up; the same phases are sent to clients. Snipers do not
use suppression fire.

Bots move with local steering and wall-following first. If a bot makes almost
no progress for 1.25 seconds it logs `AI STUCK`, requests a smoothed path from
the map's 128×128 coarse A* grid, and follows the waypoints until it has a
firing solution or reaches cover. The grid is rebuilt from static collision
geometry for each map; barrels, players and other dynamic objects are not part
of it. The fallback is a safety net for deep corners and offset gates, not a
full dynamic navigation mesh — see [`TODO.md`](TODO.md).

## Multiplayer (LAN & internet)

Up to **4 players** over a single UDP port (default **7777**), using a host-authoritative star topology — no dedicated server needed. It works on your LAN out of the box; for the internet the host opens its port automatically via UPnP (see below).

- The **host** runs the world simulation (enemies, barrels, pickups) and relays events; clients send their player state at 30 Hz and render everyone and everything else from host snapshots (20 Hz). The host also owns the current map mode, seed and world epoch; it sends a layout hash so clients rebuild the same deterministic city, verify it and follow live remaps.
- **Co-op and PvP in one mode** — everyone fights the same AI enemies, and players can damage each other. Watch your fire.
- **Optional bot infighting** — turn on **BOTS FIGHT EACH OTHER** and bots treat other bots as valid targets and damage them too. They still pick the nearest visible target, so a close player can remain the focus. Host-controlled in multiplayer.
- **Live settings menu** — `Esc` opens the settings overlay during a match without pausing: the host keeps simulating, clients keep sending state, and snapshots keep flowing. Your character stands idle (and can still be shot) while it is open. On a client, **AI ENEMIES** and **BOTS FIGHT EACH OTHER** are host-controlled and shown as `HOST`; **NEW MAP** shows `HOST ONLY`.
- **Host-controlled world changes** — `NEW MAP` resets the round for everyone and generates a fresh seed. Clients apply the new seed/epoch and keep their connection. Late joiners and reconnecting players receive the host's current live world, not the map from their own `nga.cfg`.
- Kill feed, scoreboard, name tags, and per-player colors keep the match readable.
- Dead players respawn after 3 seconds with a brief spawn shield, at a spawn point that isn't occupied.
- Clients reconnect automatically if the host restarts; peers that go silent are dropped after ~10 s.

### Host a game

```sh
./nga_strike --host            # host on UDP port 7777 (also tries UPnP)
./nga_strike --host 9000       # host on a custom port
./nga_strike --host --no-upnp  # never touch the router (LAN-only)
./nga_strike --host --no-ai    # pure PvP: no AI bots spawn at all
```

### Join a game

```sh
./nga_strike --join 192.168.1.23           # join host at default port 7777
./nga_strike --join 192.168.1.23:9000      # custom port
./nga_strike --join 192.168.1.23 --name Ahmed
```

`--name NAME` sets the name shown in the kill feed, scoreboard and overhead tags.

AI bot enemies are simulated by the host. `--no-ai` starts that run with no bots
at all (useful for pure PvP). The saved **AI ENEMIES** setting sets the default
for future launches. The saved **BOTS FIGHT EACH OTHER** setting makes bots
target and damage each other as well as players; it is off by default and is
controlled by the host in multiplayer.

### Playing over the internet (UPnP)

When you host, the game automatically asks your router to forward its UDP port
(UPnP), so friends outside your LAN can join without manual configuration:

- On success it logs a ready-to-share command, e.g.
  `NET: internet friends can join with: nga_strike --join 203.0.113.5:7777`.
- The mapping is scoped tightly: only the game's one UDP port is touched, only
  while hosting, it carries a 2-hour lease that is refreshed hourly, and it is
  removed when you quit — a crash leaves at most the remaining lease.
- `--no-upnp` disables all of this. If your router has UPnP off, or you are
  behind CGNAT / double NAT, the game says so and stays LAN-only; in that case
  a manual port forward (UDP 7777) or a mesh VPN like Tailscale/ZeroTier works.
- Packets are only accepted from the address a player joined from, so an
  internet-exposed host cannot be tricked into acting on behalf of another
  player (no forged kicks, damage or kill events).

> **Firewall:** allow inbound UDP 7777 (or your chosen port) on the host machine
> and, if you forward manually, on the router.

Running with no flags starts single-player, exactly as before.

## Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` | Move |
| Mouse | Look |
| Left Click | Fire |
| Right Click | Aim down sights / scope |
| `Shift` | Sprint |
| `Space` | Jump |
| `R` | Reload |
| `1` `2` `3` / Mouse wheel | Switch weapon |
| `Esc` | Settings menu (single-player pauses the world; multiplayer keeps it running) |
| `F6` | Toggle shadows |
| `F11` | Toggle fullscreen |

## Building & Running

### Requirements

- A C99 compiler (`cc`, `gcc` or `clang`)
- [raylib](https://www.raylib.com/) installed system-wide (tested with 6.2.0)
- [miniupnpc](https://miniupnp.tuxfamily.org/) — for automatic UPnP port mapping
  when hosting over the internet (`miniupnpc` on Arch, `libminiupnpc-dev` on
  Debian/Ubuntu)
- [ninja](https://ninja-build.org/) (optional — see manual command below)
- Linux / POSIX (the netcode uses POSIX UDP sockets)

### Build

```sh
ninja                    # release build -> ./nga_strike
ninja nga_strike_debug   # -O0 -DDEBUG build with debug symbols
ninja check              # build + run the four regression tests
./tests/run_tests.sh     # run already-built tests directly
ninja clean              # remove built binaries and test artifacts
```

`ninja check` builds `tests/test_world`, `tests/test_sites`,
`tests/test_settings` and `tests/test_enemy_nav`, then runs them through
`tests/run_tests.sh`. The suite covers deterministic cities and fixed layout
hashes, site coverage, settings parsing/sanitising, and the bot stuck/A*
fallback. Delete `tests/.check.ok` (or run `ninja -t clean`) to force a rerun.

Or without ninja:

```sh
cc -Wall -Wextra -g -O2 -std=c99 -I. \
   main.c game.c player.c weapon.c enemy.c pickup.c audio.c world.c ui.c \
   fx.c damage_fx.c shadows.c settings.c net.c upnp.c \
   -o nga_strike -lraylib -lm -lminiupnpc
```

### Run

```sh
./nga_strike
```

Run it **from the project directory**: sound files and `nga.cfg` are loaded relative to the working directory (sounds are searched in the project folder and in `./assets/`).

## Settings

Settings are stored in `nga.cfg` next to the game and saved whenever you change a persisted setting in the pause menu (`Esc`). Delete the file to reset to defaults.

| Setting | Values |
| --- | --- |
| Resolution | 1280×720, 1600×900, 1920×1080, 2560×1440 |
| Fullscreen | on / off |
| VSYNC | on / off (uncapped when off) |
| Shadows | off / low / high / ultra |
| Field of View | 50–110 |
| Sensitivity | 0.0005–0.02 |
| Volume | 0–1 |
| Point Lights | on / off |
| Distance Fog | on / off |
| World (Next Map) | CLASSIC / RANDOM 100 / RANDOM 256 — staged until the next **NEW MAP**; shows `(PENDING)` when it differs from the live map; clients show the live host world |
| New Map (Regenerate) | Generates a new seeded layout after a second confirmation press; single-player/host only and resets the round |
| Copy Map Seed | Copies the live random map's seed to the clipboard; Classic shows `COPY MAP SEED - CLASSIC` |
| AI Enemies | on / off — off despawns every bot and stops spawning; on brings them back gradually |
| Bots Fight Each Other | on / off — when on, bots also target and damage each other (nearest visible target still wins); host-controlled in multiplayer |

`world` in `nga.cfg` uses `0` for CLASSIC, `1` for RANDOM 100 and `2` for
RANDOM 256. `worldseed` is the seed used by the current random map; editing
either file by hand is optional. Changing **WORLD (NEXT MAP)** alone does not
write the file — only **NEW MAP** saves the new active mode/seed, so quitting
with a pending selection restores the previous map. Use the menu so validation
and client sync run normally.

## Project Layout

| File | Purpose |
| --- | --- |
| `main.c` | Entry point, CLI flags, window loop |
| `game.c` / `game.h` | Game state, update/draw orchestration, scoring, net event handling |
| `player.c` / `player.h` | Player movement, collision, respawn |
| `weapon.c` / `weapon.h` | Weapon definitions, shooting, viewmodel animation |
| `enemy.c` / `enemy.h` | Enemy AI, hitboxes, rendering |
| `pickup.c` / `pickup.h` | Health / ammo pickups |
| `world.c` / `world.h` | Classic arena, deterministic city generation, collision, connectivity checks and rendering |
| `shadows.c` / `shadows.h` | Shadow maps and lit shaders |
| `fx.c`, `damage_fx.c` | Particles, tracers, decals, damage numbers and indicators |
| `audio.c` / `audio.h` | Sound loading and playback helpers |
| `ui.c` / `ui.h` | HUD, pause/settings menu, multiplayer overlay |
| `settings.c` / `settings.h` | Persistent settings (`nga.cfg`) |
| `net.c` / `net.h` | LAN multiplayer (UDP, host-authoritative) |
| `tests/` | In-repo regression tests (`ninja check`) |
| `common.h` | Shared constants and structs |

## Notes

- Optional sounds `hit.mp3`, `enemy_hurt.mp3` and `pickup.mp3` are not bundled. The game logs `Missing sound: ... (using silent fallback)` and keeps going — drop the files in the project folder to enable those events.
- The world-hash fields changed the UDP wire format. Rebuild the host and every client from the same source; old and new binaries are not compatible. A `MAP HASH MISMATCH` warning means the rebuilt layouts diverged — it is diagnostic, not an automatic repair.
- See [`CHANGELOG.md`](CHANGELOG.md) for documented changes and [`AGENTS.md`](AGENTS.md) for documentation, build and commit conventions.
- See [`TODO.md`](TODO.md) for the improvement plan and known future work.

## License

MIT — see [`LICENSE`](LICENSE). Copyright (c) 2026 LowLevel6.
