# Nga-Strike — agent notes

## Documentation is mandatory

The user requires **every change or event to be documented**, not only code changes.

- Add a dated entry to [`CHANGELOG.md`](CHANGELOG.md) for every meaningful change:
  what changed, why, and how it was verified (commands, test sessions, logs).
- Update [`README.md`](README.md) whenever user-visible behavior, controls, setup
  or settings change.
- Update [`TODO.md`](TODO.md) when a roadmap item is completed, a new issue is
  found, or work is deferred.
- Record bugs found during testing, design decisions, failed experiments and
  process changes — not just successful features.
- Include commit hashes when available.

## Build and verify

- Build with `ninja` in the repo root.
- Single-player smoke test: launch, press `Esc`, confirm the menu opens/closes
  and the world pauses.
- Multiplayer smoke test: run two instances
  (`--host <port> --no-upnp` and `--join 127.0.0.1:<port>`), confirm they
  connect, open both menus and verify there are no peer timeouts.
- For menu/settings changes, verify a real `nga.cfg` round-trip (including
  `sens` and `volume`, which are floats).

## Git

- Branch: `main`; remote: `git@github.com:Ahmed234567890/Nga-strike.git`.
- Never commit local `nga.cfg` or the untracked `nga_strike_debug` binary.
- The built `nga_strike` binary and `.ninja_log` are tracked and are committed
  with code changes, matching existing repo practice.
- Commit and push once a change is verified.
