# Runtime Map Model Notes

Research/implementation cycle: 2026-04-24 22:28-23:10 CDT  
Scope: first pure C `SetupGameLevel`-style data model built from decoded WL6 map 0 planes, plus renderer-facing sprite-reference selection.

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
- `source/original/WOLFSRC/WL_ACT1.C::statinfo`
  - maps static object types to `SPR_STAT_*` shapenum values and blocking/bonus/treasure behavior.
- `source/original/WOLFSRC/WL_ACT2.C::SpawnStand`, `SpawnPatrol`, and `SpawnDeadGuard`
  - live guards/dogs increment kill total; dead guards are inert and do not.
  - patrol spawns shift their active tile one step in the source direction.
  - medium/hard info-plane tiles are difficulty-gated.
  - initial guard/dog/dead-guard states carry `SPR_GRD_S_1`, `SPR_GRD_W1_1`, `SPR_DOG_W1_1`, and `SPR_GRD_DEAD` shapenum values.

## Implemented seam

Added a data-only runtime model outside `source/original/`:

- `source/modern-c-sdl3/include/wl_game_model.h`
- `source/modern-c-sdl3/src/wl_game_model.c`

The model currently captures:

- `tilemap[64*64]` initialized like the original wall pass plus door center/side markers and ambush clearing.
- player spawn descriptor.
- door descriptors with orientation, lock, and adjacent area connectivity.
- static descriptors with blocking/bonus/treasure flags.
- pushwall/secret markers.
- patrol path markers.
- difficulty-filtered actor descriptors for WL6 guard, officer, SS, dog, mutant, boss, and dead-guard tiles.
- kill, treasure, and secret totals.
- renderer-facing scene sprite references for statics and initial actor states, including shapenum, VSWAP chunk index, 16.16 world center, validated surface-cache input chunks, and deterministic combined-scene render input.

This is intentionally not a live game loop yet. It is a deterministic setup model that future gameplay, collision, and renderer code can consume.

## WL6 map 0 committed assertions

Easy difficulty model:

- player: present at `(29,57)`, facing east.
- doors: `22`; first door `(28,11)`, vertical lock `0`, connects areas `36` and `35`; lock `5` door at descriptor `17` from source tile `100`, connects areas `33` and `0`.
- door connectivity: `22` door links, `17` unique area-pairs; pair `35<->36` appears twice, pair `2<->2` appears four same-area doors (eight symmetric matrix increments), pair `0<->33` appears once.
- statics: `121`; blocking `34`; bonus `48`; treasure total `23`.
- path markers: `18`; first marker `(2,33)` from tile `96`.
- pushwalls/secrets: `5`; first pushwall `(10,13)`.
- actors: `12`; kill total `11`; includes one inert dead guard not counted for kills.
- first actor: guard at `(33,9)` from tile `111`.
- first dog patrol actor: source `(54,45)`, active tile shifted to `(55,45)`.
- unknown info-plane tiles after gating: `0`.
- scene sprite references: `133` total (`121` statics + `12` actors), first static ref `model_index=0`, shapenum `14`, VSWAP chunk `120`, world center `(0x1d8000,0x088000)`, representative patrol guard ref shapenum `58` / chunk `164`, dog ref shapenum `99` / chunk `205`, dead guard ref shapenum `95` / chunk `201`, descriptor hash `0x2ab36473`; representative surface-cache hashes `0x38769770`, `0xbd6176ba`, `0x0fe580fa`, `0xa875d685`, `0x63f7eba2`, combined cache hash `0x4a8eb8db`; selected additional maps now assert zero unknown easy-difficulty runtime info tiles and scene-ref hashes for SS coverage (`Wolf1 Map2`, `0xab87ed41`), mutant coverage (`Wolf2 Map1`, `0x89b8f3c0`), and officer coverage (`Wolf3 Map1`, `0xc090c2df`), and a boss-map seam (`Wolf2 Boss`, `0xb2dab28b`); visible runtime-ref scene render using refs `113`/`114` yields cache hashes `0x442facd4`/`0xd363bf0c`, combined cache hash `0xd53b06f5`, sorted projected sprite order `26` then `16`, and canvas hash `0x61f7f78b`; broader visible refs `110`, `111`, `113`, `114`, and `115` yield combined cache hash `0x61a879ca`, east-facing sorted source order `21,11,26,16,16` with canvas hash `0xb92e568b`, and a northeast camera variant with canvas hash `0x4668f191`.

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
asset/decompression/semantics/model/vswap/boss-map-scene tests passed for game-files/base
```

## Cycle update: door-area connectivity

The door model now stores the original `DoorOpening`/`DoorClosing` area pair for each door. For vertical doors this is right/left; for horizontal doors this is up/down. Tests also populate a symmetric connectivity-count matrix representing what the original `areaconnect` matrix would receive as doors open.

## Cycle update: renderer sprite refs

Added `wl_scene_sprite_ref` and `wl_collect_scene_sprite_refs`, bridging the runtime model to the renderer without decoding or committing asset bytes. Static refs map `statinfo`-style types to `SPR_STAT_*` shapenum values; actor refs map initial guard/officer/SS/dog/mutant/boss/dead-guard states to their starting shapenum values. Each ref includes model kind/index, shapenum source index, VSWAP chunk index, and 16.16 world-center coordinates. The current tests feed representative and broader visible ref chunk indices into the sprite surface-cache decoder, then pass cached ref surfaces and ref world coordinates directly into `wl_render_camera_scene_view`, asserting sorted projection descriptors and final indexed-scene hashes across multiple camera vectors without committing decoded asset bytes.

## Next step

Next, connect runtime-scene/palette seams to future live gameplay events, add more map coverage, or add a small SDL3 presentation seam when SDL3 is available. Keep verification headless and deterministic.


## Cycle update: multi-map enemy runtime coverage

Extended the runtime model beyond guard/dog/dead-guard handling to spawn original-style officer, SS, and mutant easy/medium/hard stand/patrol tile ranges, preserving the original difficulty gates and patrol tile shifts. Scene sprite refs now expose the corresponding initial stand/path sprites (`SPR_OFC_*`, `SPR_SS_*`, `SPR_MUT_*`) through VSWAP chunk indices. The headless WL6 tests now cover `Wolf1 Map2` for SS refs, `Wolf2 Map1` for mutant refs, and `Wolf3 Map1` for officer refs, asserting model counts, zero unknown info tiles, actor-kind counts, scene-ref counts, and scene-ref descriptor hashes.


## Cycle update: boss-map runtime coverage

Added WL6 boss tile handling for the non-Spear boss spawns (`SpawnBoss`, `SpawnSchabbs`, `SpawnGift`, `SpawnFat`, `SpawnFakeHitler`, `SpawnHitler`, and `SpawnGretel`) as data-only runtime actors. The model records their source tile, original north/south starting direction, shootable/kill-total behavior, and renderer-facing starting sprite when known. Headless coverage now includes `Wolf2 Boss`, asserting map/model totals, two mutants plus one boss, zero unknown info tiles, scene-ref count `168`, and descriptor hash `0xb2dab28b`.
