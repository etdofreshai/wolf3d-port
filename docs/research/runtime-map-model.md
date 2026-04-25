# Runtime Map Model Notes

Research/implementation cycle: 2026-04-24 22:28-23:10 CDT  
Scope: first pure C `SetupGameLevel`-style data model built from decoded WL6 map 0 planes.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/WL_GAME.C::SetupGameLevel`
  - initializes `tilemap` from wall plane: tiles `< AREATILE` are solid entries, tiles `>= AREATILE` become floor `0`.
  - scans wall plane for door tiles `90..101` and calls `SpawnDoor`.
  - scans the info plane through `ScanInfoPlane`.
  - clears `AMBUSHTILE` markers after actor spawning.
- `source/original/WOLFSRC/WL_ACT1.C::SpawnDoor`
  - stores a door center as `door_number | 0x80` in `tilemap`.
  - marks neighboring side tiles with `0x40`.
  - even door tiles are vertical; odd door tiles are horizontal; lock index is derived from the tile pair.
- `source/original/WOLFSRC/WL_ACT1.C::SpawnStatic`
  - static objects can be blocking, bonus, treasure, or dressing.
- `source/original/WOLFSRC/WL_ACT2.C::SpawnStand`, `SpawnPatrol`, and `SpawnDeadGuard`
  - live guards/dogs increment kill total; dead guards are inert and do not.
  - patrol spawns shift their active tile one step in the source direction.
  - medium/hard info-plane tiles are difficulty-gated.

## Implemented seam

Added a data-only runtime model outside `source/original/`:

- `source/modern-c-sdl3/include/wl_game_model.h`
- `source/modern-c-sdl3/src/wl_game_model.c`

The model currently captures:

- `tilemap[64*64]` initialized like the original wall pass plus door center/side markers and ambush clearing.
- player spawn descriptor.
- door descriptors with orientation and lock.
- static descriptors with blocking/bonus/treasure flags.
- pushwall/secret markers.
- patrol path markers.
- difficulty-filtered actor descriptors for WL6 map 0 guard/dog/dead-guard tiles.
- kill, treasure, and secret totals.

This is intentionally not a live game loop yet. It is a deterministic setup model that future gameplay, collision, and renderer code can consume.

## WL6 map 0 committed assertions

Easy difficulty model:

- player: present at `(29,57)`, facing east.
- doors: `22`; first door `(28,11)`, vertical lock `0`; lock `5` door at descriptor `17` from source tile `100`.
- statics: `121`; blocking `34`; bonus `48`; treasure total `23`.
- path markers: `18`; first marker `(2,33)` from tile `96`.
- pushwalls/secrets: `5`; first pushwall `(10,13)`.
- actors: `12`; kill total `11`; includes one inert dead guard not counted for kills.
- first actor: guard at `(33,9)` from tile `111`.
- first dog patrol actor: source `(54,45)`, active tile shifted to `(55,45)`.
- unknown info-plane tiles after gating: `0`.

Difficulty-gating assertions:

- medium: actors `21`, kill total `20`, unknown info tiles `0`.
- hard: actors `38`, kill total `37`, unknown info tiles `0`.

## Verification evidence

Command:

```bash
cd source/modern-c-sdl3
make clean test
```

Observed result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model tests passed for game-files/base
```

## Next step

Deepen the runtime model with door-area connectivity and/or move to VSWAP chunk descriptors. Door-area connectivity is the most direct continuation of `SetupGameLevel`, while VSWAP parsing starts the sprite/wall asset path needed for rendering.
