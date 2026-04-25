# Autopilot State

Status: initialized

## Current Phase

Project setup and automation design.

## Latest Verified Milestone

- Repo exists under `~/.openclaw/tmp/Wolfenstein 3D port`.
- Original id Software Wolfenstein 3D source is available at `source/original/` as a submodule.
- Local purchased game data is present under `game-files/base/` and ignored by git.
- Autopilot operating rules are documented in `docs/AUTOPILOT.md`.

## Current Strategy

Use an adaptive TaskFlow-backed loop: observe, evaluate, act, verify, record, continue. Avoid asking ET for routine decisions; make best guesses and document rationale.

## Next Likely Move

Define the first concrete technical milestone: inventory original source architecture and data files, then choose the first implementation target.

## Blockers

None.
