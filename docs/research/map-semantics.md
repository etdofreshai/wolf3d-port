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


## Cycle update: live actor AI patrol tick

Runtime patrol metadata now participates in a live frame boundary, not only isolated helpers. The headless test verifies a patrol actor moves after the live tick, standing actors are skipped, blocked patrols report aggregate blocked state, and negative tics are rejected.


## Cycle update: live AI patrol scene refs

Patrol path semantics now flow through live AI ticking into scene refs. The test confirms the moved patrol actor updates world-center coordinates and blocked movement preserves the last renderer-facing location.


## Cycle update: live AI patrol rendering

Patrol path semantics now carry through live AI movement, scene-ref collection, VSWAP sprite decoding, and headless scene rendering. This confirms movement metadata remains presentation-ready after the live frame wrapper.


## Cycle update: patrol fine-position refs

Tile-granular patrol semantics now have partial movement metadata. A half-tile patrol budget keeps the actor on its tile but advances renderer-facing world coordinates, matching the path toward original `T_Path` distance accumulation.


## Cycle update: live AI fine-position patrol rendering

Path/tic patrol semantics now pass through the full live AI presentation path at sub-tile precision. A partial movement budget updates renderer-facing world coordinates without consuming a whole tile.


## Cycle update: patrol remainder accumulation

The patrol path/tic seam now models original-style distance accumulation across calls instead of treating each frame independently. Partial budgets persist until they form a whole tile step.


## Cycle update: live AI remainder rendering

Patrol path semantics now accumulate partial movement through multiple live AI frames, then render the resulting whole-tile step. This closes the loop from path/tic state to presentation-ready refs.


## Cycle update: live AI dog fine-position rendering

Patrol semantics now feed live AI fine-position rendering for a second actor class. Dog patrol starts retain the same path/tic contract while selecting dog sprite metadata.


## Cycle update: live AI officer fine-position rendering

Patrol semantics now feed live AI fine-position rendering for officers too. The same path/tic contract selects officer walk sprite metadata while preserving sub-tile placement.


## Cycle update: live AI SS fine-position rendering

Patrol semantics now feed live AI fine-position rendering for SS actors too. The same path/tic contract selects SS walk sprite metadata while preserving sub-tile placement.


## Cycle update: live AI mutant fine-position rendering

Patrol semantics now feed live AI fine-position rendering for mutants too. The same path/tic contract selects mutant walk sprite metadata while preserving sub-tile placement.


## Cycle update: live AI boss fine-position rendering

Live AI path/tic semantics now feed renderer-facing boss refs at sub-tile precision, preserving boss source-tile sprite selection while reusing the same deterministic half-tile movement contract.


## Cycle update: chase direction selection seam

Map blocking now participates in chase direction selection. The deterministic seam models the original preference ordering without random global state by taking an explicit search-order flag.
