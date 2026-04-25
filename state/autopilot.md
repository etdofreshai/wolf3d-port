# Autopilot State

Status: initialized

## Current Phase

Initial source inventory and test strategy for modern C + SDL3 port.

## Latest Verified Milestone

- Repo exists under `~/.openclaw/tmp/Wolfenstein 3D port`.
- Original id Software Wolfenstein 3D source is available at `source/original/` as a submodule.
- Local purchased game data is present under `game-files/base/` and ignored by git.
- Autopilot operating rules are documented in `docs/AUTOPILOT.md`.

## Current Strategy

Use an adaptive TaskFlow-backed loop focused on a faithful modern C + SDL3 port. Use tests as the bridge between original behavior and modern implementation. Avoid asking ET for routine decisions; make best guesses and document rationale.

## Next Likely Move

Inventory original source architecture and data files, then design the first cross-implementation test harness strategy.

## Blockers

None.
