# Nga-Strike

A fast-paced arena first-person shooter built from scratch in C with [raylib](https://www.raylib.com/).

Play solo against AI enemies, or jump into a LAN match with up to 4 players — co-op and PvP at the same time.

## Features

- **3 weapons** — semi-auto Pistol, automatic AKR, and a hard-hitting scoped Sniper, each with its own damage, spread, recoil, ADS behavior and reloads.
- **Enemy AI with personality** — patrol / chase / strafe / cover / flee behavior, three combat roles (assault, marksman, rusher), elite variants, melee charges with a visible wind-up, and helmets you can shoot off.
- **Proper hit zones** — head, torso, leg, and helmet hitboxes with damage numbers, hitmarkers, and headshot streak bonuses.
- **Explosive barrels** — shoot them to clear a room. They hurt you too.
- **Pickups** — health and ammo crates that respawn on a timer.
- **Movement** — sprint, jump, aim-down-sights with per-weapon FOV and sensitivity, and a spawn shield after respawning.
- **Rendering** — dynamic shadow maps (off / low / high / ultra), colored point lights, distance fog, particles, tracers, bullet decals and animated first-person viewmodels.
- **HUD** — health, kills, score, timer, reload/ammo display, hitmarkers, headshot banners, and directional damage indicators.

## Multiplayer (LAN)

Up to **4 players** over a single UDP port (default **7777**), using a host-authoritative star topology — no dedicated server needed.

- The **host** runs the world simulation (enemies, barrels, pickups) and relays events; clients send their player state at 30 Hz and render everyone and everything else from host snapshots (20 Hz).
- **Co-op and PvP in one mode** — everyone fights the same AI enemies, and players can damage each other. Watch your fire.
- Kill feed, scoreboard, name tags, and per-player colors keep the match readable.
- Dead players respawn after 3 seconds with a brief spawn shield, at a spawn point that isn't occupied.
- Clients reconnect automatically if the host restarts; peers that go silent are dropped after ~10 s.

### Host a game

```sh
./nga_strike --host            # host on UDP port 7777
./nga_strike --host 9000       # host on a custom port
```

### Join a game

```sh
./nga_strike --join 192.168.1.23           # join host at default port 7777
./nga_strike --join 192.168.1.23:9000      # custom port
./nga_strike --join 192.168.1.23 --name Ahmed
```

`--name NAME` sets the name shown in the kill feed, scoreboard and overhead tags.

> **Firewall:** allow inbound UDP 7777 (or your chosen port) on the host machine.

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
| `Esc` | Pause menu & settings (single-player only — pausing would freeze networking in MP) |
| `F6` | Toggle shadows |
| `F11` | Toggle fullscreen |

## Building & Running

### Requirements

- A C99 compiler (`cc`, `gcc` or `clang`)
- [raylib](https://www.raylib.com/) installed system-wide (tested with 6.2.0)
- [ninja](https://ninja-build.org/) (optional — see manual command below)
- Linux / POSIX (the netcode uses POSIX UDP sockets)

### Build

```sh
ninja                    # release build -> ./nga_strike
ninja nga_strike_debug   # -O0 -DDEBUG build with debug symbols
ninja clean              # remove built binaries
```

Or without ninja:

```sh
cc -Wall -Wextra -g -O2 -std=c99 \
   main.c game.c player.c weapon.c enemy.c pickup.c audio.c world.c ui.c \
   fx.c damage_fx.c shadows.c settings.c net.c \
   -o nga_strike -lraylib -lm
```

### Run

```sh
./nga_strike
```

Run it **from the project directory**: sound files and `nga.cfg` are loaded relative to the working directory (sounds are searched in the project folder and in `./assets/`).

## Settings

Settings are stored in `nga.cfg` next to the game and saved whenever you change something in the pause menu (`Esc`). Delete the file to reset to defaults.

| Setting | Values |
| --- | --- |
| Resolution | 1280×720, 1600×900, 1920×1080, 2560×1440 |
| Fullscreen | on / off |
| Shadows | off / low / high / ultra |
| Field of View | 50–110 |
| Sensitivity | 0.0005–0.02 |
| Volume | 0–1 |
| Point Lights | on / off |
| Distance Fog | on / off |

## Project Layout

| File | Purpose |
| --- | --- |
| `main.c` | Entry point, CLI flags, window loop |
| `game.c` / `game.h` | Game state, update/draw orchestration, scoring, net event handling |
| `player.c` / `player.h` | Player movement, collision, respawn |
| `weapon.c` / `weapon.h` | Weapon definitions, shooting, viewmodel animation |
| `enemy.c` / `enemy.h` | Enemy AI, hitboxes, rendering |
| `pickup.c` / `pickup.h` | Health / ammo pickups |
| `world.c` / `world.h` | Arena geometry, collision, rendering |
| `shadows.c` / `shadows.h` | Shadow maps and lit shaders |
| `fx.c`, `damage_fx.c` | Particles, tracers, decals, damage numbers and indicators |
| `audio.c` / `audio.h` | Sound loading and playback helpers |
| `ui.c` / `ui.h` | HUD, pause/settings menu, multiplayer overlay |
| `settings.c` / `settings.h` | Persistent settings (`nga.cfg`) |
| `net.c` / `net.h` | LAN multiplayer (UDP, host-authoritative) |
| `common.h` | Shared constants and structs |

## Notes

- Optional sounds `hit.mp3`, `enemy_hurt.mp3` and `pickup.mp3` are not bundled. The game logs `Missing sound: ... (using silent fallback)` and keeps going — drop the files in the project folder to enable those events.
- See [`TODO.md`](TODO.md) for the improvement plan and known future work.

## License

MIT — see [`LICENSE`](LICENSE). Copyright (c) 2026 LowLevel6.
