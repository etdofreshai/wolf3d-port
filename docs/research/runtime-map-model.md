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
- difficulty-filtered actor descriptors for WL6 guard, officer, SS, dog, mutant, boss, ghost, and dead-guard tiles.
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
- scene sprite references: `133` total (`121` statics + `12` actors), first static ref `model_index=0`, shapenum `14`, VSWAP chunk `120`, world center `(0x1d8000,0x088000)`, representative patrol guard ref shapenum `58` / chunk `164`, dog ref shapenum `99` / chunk `205`, dead guard ref shapenum `95` / chunk `201`, descriptor hash `0x2ab36473`; representative surface-cache hashes `0x38769770`, `0xbd6176ba`, `0x0fe580fa`, `0xa875d685`, `0x63f7eba2`, combined cache hash `0x4a8eb8db`; selected additional maps now assert zero unknown easy-difficulty runtime info tiles and scene-ref hashes for SS coverage (`Wolf1 Map2`, `0xab87ed41`), mutant coverage (`Wolf2 Map1`, `0x89b8f3c0`), and officer coverage (`Wolf3 Map1`, `0xc090c2df`), a boss-map seam (`Wolf2 Boss`, `0xb2dab28b`), and ghost-map coverage (`Wolf3 Secret`, `0xe03fdb45`); visible runtime-ref scene render using refs `113`/`114` yields cache hashes `0x442facd4`/`0xd363bf0c`, combined cache hash `0xd53b06f5`, sorted projected sprite order `26` then `16`, and canvas hash `0x61f7f78b`; broader visible refs `110`, `111`, `113`, `114`, and `115` yield combined cache hash `0x61a879ca`, east-facing sorted source order `21,11,26,16,16` with canvas hash `0xb92e568b`, and a northeast camera variant with canvas hash `0x4668f191`.

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
asset/decompression/semantics/model/vswap/door-progression tests passed for game-files/base
```

## Cycle update: door-area connectivity

The door model now stores the original `DoorOpening`/`DoorClosing` area pair for each door. For vertical doors this is right/left; for horizontal doors this is up/down. Tests also populate a symmetric connectivity-count matrix representing what the original `areaconnect` matrix would receive as doors open.

## Cycle update: renderer sprite refs

Added `wl_scene_sprite_ref` and `wl_collect_scene_sprite_refs`, bridging the runtime model to the renderer without decoding or committing asset bytes. Static refs map `statinfo`-style types to `SPR_STAT_*` shapenum values; actor refs map initial guard/officer/SS/dog/mutant/boss/ghost/dead-guard states to their starting shapenum values. Each ref includes model kind/index, shapenum source index, VSWAP chunk index, and 16.16 world-center coordinates. The current tests feed representative and broader visible ref chunk indices into the sprite surface-cache decoder, then pass cached ref surfaces and ref world coordinates directly into `wl_render_camera_scene_view`, asserting sorted projection descriptors and final indexed-scene hashes across multiple camera vectors without committing decoded asset bytes.

## Next step

Next, connect runtime-scene/palette seams to future live gameplay events, add more map coverage, or add a small SDL3 presentation seam when SDL3 is available. Keep verification headless and deterministic.


## Cycle update: multi-map enemy runtime coverage

Extended the runtime model beyond guard/dog/dead-guard handling to spawn original-style officer, SS, and mutant easy/medium/hard stand/patrol tile ranges, preserving the original difficulty gates and patrol tile shifts. Scene sprite refs now expose the corresponding initial stand/path sprites (`SPR_OFC_*`, `SPR_SS_*`, `SPR_MUT_*`) through VSWAP chunk indices. The headless WL6 tests now cover `Wolf1 Map2` for SS refs, `Wolf2 Map1` for mutant refs, and `Wolf3 Map1` for officer refs, asserting model counts, zero unknown info tiles, actor-kind counts, scene-ref counts, and scene-ref descriptor hashes.


## Cycle update: boss-map runtime coverage

Added WL6 boss tile handling for the non-Spear boss spawns (`SpawnBoss`, `SpawnSchabbs`, `SpawnGift`, `SpawnFat`, `SpawnFakeHitler`, `SpawnHitler`, and `SpawnGretel`) as data-only runtime actors. The model records their source tile, original north/south starting direction, shootable/kill-total behavior, and renderer-facing starting sprite when known. Headless coverage now includes `Wolf2 Boss`, asserting map/model totals, two mutants plus one boss, zero unknown info tiles, scene-ref count `168`, and descriptor hash `0xb2dab28b`.


## Cycle update: ghost-map runtime coverage

Added WL6 ghost tile handling for `SpawnGhosts` (`en_blinky`, `en_clyde`, `en_pinky`, and `en_inky`) as data-only runtime actors. The model preserves the original east starting direction, non-shootable flag, ambush-style mode, and kill-total increment. Scene sprite refs now map ghost source tiles `224..227` to their starting `SPR_*_W1` sprite indices. Headless coverage includes `Wolf3 Secret`, asserting four ghosts, model totals, zero unknown info tiles, scene-ref count `134`, and descriptor hash `0xe03fdb45`.


## Cycle update: player gameplay event seam

Added `wl_gameplay`, a pure C state seam for future live gameplay. It currently covers player health/death, baby-difficulty damage scaling, god/victory damage handling, bonus/damage palette-flash starts, healing clamp, score awards, and extra-life threshold progression. This gives the runtime and renderer/palette seams a deterministic event source before a full actor/player loop exists.


## Cycle update: bonus pickup event seam

Expanded `wl_gameplay` from player damage/score events into bonus pickup semantics that future static-object collision can call. It now models ammo, weapons, keys, treasure pickups, health pickups, full-heal, and Spear completion while keeping renderer/status/sound effects out of the deterministic state seam.


## Cycle update: static pickup/removal seam

Runtime static descriptors now carry an `active` flag. `wl_try_pickup_static_bonus` bridges those descriptors to `wl_gameplay` bonus handling, clears active statics only after successful pickup, and leaves rejected pickups (for example full-health food) visible. `wl_collect_scene_sprite_refs` skips inactive statics, so headless tests can verify both gameplay state changes and renderer-facing removal by checking the scene-ref count drops after a food pickup.


## Cycle update: visible static pickup probe

Added `wl_try_pickup_visible_static_bonus`, a headless gameplay/render seam based on the original `WL_DRAW.C::TransformTile` pickup test used before drawing bonus statics. The probe scans active bonus statics, applies the same near/in-front/lateral pickup window using 16.16 world coordinates and a view vector, then routes the touched static through `wl_try_pickup_static_bonus`. Tests cover an out-of-lateral-range miss, an in-range full-health food rejection that leaves the static active, and a successful in-range food pickup that heals, starts the bonus flash, deactivates the static, and removes it from later scene refs.


## Cycle update: player motion/pickup tick

Added a small headless player motion seam around the existing gameplay/model data. `wl_init_player_motion_from_spawn` creates the original `SpawnPlayer`-style centered 16.16 position from the runtime model's player start, and `wl_step_player_motion` mirrors the original `ClipMove` fallback shape: try combined X/Y movement, then X-only, then Y-only, otherwise stay put. The collision check uses `PLAYERSIZE`/`MINDIST`-style bounds against solid model tiles, closed door centers, and active blocking statics, then runs the visible static pickup probe after the movement attempt. Tests now cover spawn centering from WL6 map 0, a moving step that picks up food and deactivates it through the gameplay path, and a blocking-static collision that prevents movement.


## Cycle update: use-button dispatch seam

Added a headless `Cmd_Use`-style dispatch seam in `wl_use_player_facing`. The helper derives the cardinal target tile from the player's current tile and facing direction, detects pushwall markers from the info plane, flips east/west elevator switches and marks level completion/secret completion from the wall plane, and applies `OperateDoor`-style locked-door/opening/closing transitions to runtime door descriptors. Door descriptors now carry the original `dr_open/dr_closed/dr_opening/dr_closing` action state. Tests cover pushwall target metadata, secret elevator completion and switch flip, locked-door rejection without a key, keyed opening, and closing an already-opening door.


## Cycle update: door progression seam

Added `wl_step_doors`, a deterministic headless door update seam mirroring the original `MoveDoors`/`DoorOpening`/`DoorOpen`/`DoorClosing` state transitions. Runtime door descriptors now retain `doorposition` and `ticcount` equivalents. Opening doors advance by `tics << 10`, become fully open at `0xffff`, reset ticcount, and release player collision by clearing the door-center tile. Open doors accumulate ticcount until `OPENTICS=300`, then begin closing and restore the solid door-center marker. Closing doors decrement by `tics << 10`, close at zero, and reopen if the player overlaps the doorway while closing. Tests cover partial opening, full opening/collision release, open wait, close start/collision restore, blocked close reopen, and full close.
