# Map Semantic Classification Notes

Research/implementation cycle: 2026-04-24 22:27-22:55 CDT  
Scope: first faithful pure C semantic characterization of decoded WL6 map 0 planes.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/WL_DEF.H` tile constants:
  - `ICONARROWS = 90`
  - `PUSHABLETILE = 98`
  - `EXITTILE = 99`
  - `AMBUSHTILE = 106`
  - `AREATILE = 107`
  - `ELEVATORTILE = 21`
  - direction constants `NORTH/EAST/SOUTH/WEST = 0/1/2/3`
- `source/original/WOLFSRC/WL_GAME.C::SetupGameLevel`:
  - wall-plane tiles `< AREATILE` are copied to `tilemap`/`actorat` as solid.
  - wall-plane tiles `>= AREATILE` are area floors.
  - door wall tiles `90..101` call `SpawnDoor`; even tiles are vertical, odd tiles horizontal, lock is `(tile - 90-or-91) / 2`.
  - `AMBUSHTILE` markers are cleared after actor spawning.
- `source/original/WOLFSRC/WL_GAME.C::ScanInfoPlane`:
  - `19..22` player starts, direction `NORTH + tile - 19`.
  - `23..74` static objects, type `tile - 23`.
  - `98` pushwall marker / secret total.
  - guard/dog/officer/SS/mutant/boss/ghost ranges dispatch to spawn functions, with medium/hard difficulty gates.
- `source/original/WOLFSRC/WL_ACT1.C::statinfo` and `SpawnStatic`:
  - static object type controls blocking, bonus, and treasure counting.
- `source/original/WOLFSRC/WL_ACT2.C::SelectPathDir`:
  - object-plane `ICONARROWS..ICONARROWS+7` are patrol path markers.

## Implemented seam

Added a pure C semantic classifier outside `source/original/`:

- `source/modern-c-sdl3/include/wl_map_semantics.h`
- `source/modern-c-sdl3/src/wl_map_semantics.c`

The classifier consumes already-decoded 64x64 wall/info planes and emits `wl_map_semantics` counts. It intentionally does not spawn live actors yet; it creates stable, headless assertions that future `SetupGameLevel` work can match.

## WL6 map 0 committed assertions

Wall plane:

- solid tiles: `3082`
- area tiles: `1014`
- doors: `22` total, `14` vertical, `8` horizontal
- door locks: lock `0` count `20`, lock `5` count `2`
- ambush tiles: `3`
- elevator tiles: `6`
- alternate elevator/area tile `107`: `1`

Info plane:

- player starts: `1`, first at `(29,57)` facing east
- static objects: `121`
- static blockers: `34`
- static bonuses: `48`
- static treasures: `23`
- pushwall markers/secrets: `5`
- patrol path markers: `18`
- guard starts: easy `10`, medium `7`, hard `15`
- dog starts: easy `1`, medium `2`, hard `2`
- dead guards: `1`
- officers/SS/mutants/bosses/ghosts: `0`
- unknown nonzero info-plane tiles: `0`

These are metadata/semantic counts only; no proprietary map bytes are committed.

## Verification evidence

Command:

```bash
cd source/modern-c-sdl3
make test
```

Observed result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics tests passed for game-files/base
```

## Next step

Use these semantic counts as the oracle for a minimal `SetupGameLevel`-style runtime map model: decoded tilemap, door descriptors, static descriptors, player spawn, and difficulty-filtered actor descriptors. Keep it pure C/headless before SDL3 presentation.


## Cycle update: patrol path direction seam

Path marker metadata now feeds a small runtime decision seam. Cardinal `ICONARROWS` markers can select an actor patrol direction, while existing non-cardinal markers are preserved for later diagonal/path fidelity instead of being forced into an invalid cardinal move.


## Cycle update: patrol actor step seam

Path marker semantics now feed a small actor movement mutation. Headless tests cover marker-directed one-tile patrol stepping, continuation after leaving a marker, blocked next-tile handling, and non-patrol rejection without modifying original source.


## Cycle update: patrol actor tic stepping

Patrol path metadata now participates in a tic-budgeted actor movement seam. The headless test verifies partial movement, two-tile advancement from `speed * tics`, blocked-tile stop behavior, and invalid negative tic rejection.


## Cycle update: patrol actors batch tick

Patrol actor metadata now feeds a batch runtime AI tick. Headless tests verify one patrol actor moves, one blocked patrol actor stays put, and a standing actor is skipped while aggregate step/block counts remain deterministic.
