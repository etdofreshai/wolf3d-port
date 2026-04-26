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
asset/decompression/semantics/model/vswap/runtime-live-actor-tick tests passed for game-files/base
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

Added WL6 boss tile handling for the non-Spear boss spawns (`SpawnBoss`, `SpawnSchabbs`, `SpawnGift`, `SpawnFat`, `SpawnFakeHitler`, `SpawnHitler`, and `SpawnGretel`) as data-only runtime actors. The model records their source tile, original north/south starting direction, shootable/kill-total behavior, and renderer-facing starting sprite when known. Headless coverage now includes `Wolf1 Boss`, `Wolf2 Boss`, `Wolf3 Boss`, `Wolf4 Boss`, `Wolf5 Boss`, and `Wolf6 Boss`, asserting map/model totals, zero unknown info tiles, actor-kind counts, scene-ref counts/hashes, and broader boss mixes (mutants, guards, officers, SS, and boss actors). `Wolf1 Boss` and `Wolf5 Boss` also verify info-plane exit trigger markers so those original map specials no longer appear as unknown runtime info tiles.


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


## Cycle update: pushwall progression seam

Added runtime pushwall motion state plus `wl_start_pushwall` and `wl_step_pushwall`, mirroring the original `PushWall`/`MovePWalls` data transitions headlessly. Starting a pushwall now rejects concurrent pushes, requires a solid source tile and clear destination, increments the player's secret count, marks the source tile with the original `0xc0` moving bits, and reserves the next tile. Progression advances `pwallstate` by tics, derives `pwallpos=(state/2)&63`, clears the vacated tile when crossing a 128-tic block boundary, shifts the moving marker forward one tile, reserves the second tile, and stops after the second block crossing while leaving the final wall tile in place. Tests cover start metadata, secret count, sub-tile position, first tile crossing, reserved destination, and final stop.


## Cycle update: live solid-plane raycast bridge

Added `wl_build_runtime_solid_plane`, a renderer/raycast-facing bridge from mutable runtime `tilemap` state to the existing wall-ray helpers. The bridge converts clear floor and door-side markers to empty space, keeps normal wall tiles as-is, maps closed door centers to a caller-selected placeholder wall tile until door rendering exists, and preserves moving pushwall wall ids from the low six bits of the original `0xc0` moving marker. Tests now prove live door state affects ray hits: a closed door blocks an east ray, clearing that door lets the ray continue to a later wall, and a moving pushwall marker becomes the first ray hit with its original wall id.


## Cycle update: live runtime wall rendering

Added `wl_render_runtime_camera_wall_view`, a headless renderer-facing wrapper that builds the runtime solid plane from `wl_game_model::tilemap` and feeds it into the existing camera wall-view renderer. The committed test uses a tiny synthetic live model to prove render-visible state changes: a closed door center blocks three camera rays and hashes to `0x62d02b0d`; clearing the same door makes those rays continue to a later wall and hashes to `0x32a9148e`. This keeps door/pushwall animation state on the path toward SDL3 presentation without introducing SDL yet.


## Cycle update: door wall descriptors

Added `wl_build_door_wall_hit`, a renderer-facing descriptor seam for original-style doors. It uses `DOORWALL = PMSpriteStart - 8`, picks normal/locked/elevator door page groups from the runtime door lock, applies the vertical-door page offset, and computes texture offsets as `((intercept - doorposition) >> 4) & 0xfc0`, matching `WL_DRAW.C::HitHorizDoor`/`HitVertDoor`. Tests assert normal and locked vertical page/texture descriptors and render a locked door strip with canvas hash `0x40d8b9a5` without storing decoded door pixels.


## Cycle update: runtime door-aware ray hits

Added `wl_cast_runtime_fixed_wall_ray`, a live-model DDA seam that reads `wl_game_model::tilemap` instead of immutable wall planes. It skips empty floor and door-side markers, preserves moving pushwall wall ids, and resolves door-center markers through the runtime `wl_door_desc` array so door hits emit the same page/texture metadata as `wl_build_door_wall_hit` with `doorposition` applied. Tests assert a locked opening door hit (`page 105`, texture `0x0400`, distance `0x8000`), the same ray after the door is cleared continuing to a later wall (`page 3`, distance `0x28000`), and a moving pushwall marker producing wall page `73`.


## Cycle update: door-aware runtime camera rendering

Added `wl_render_runtime_door_camera_wall_view`, which feeds camera-generated rays through `wl_cast_runtime_fixed_wall_ray` instead of the immutable wall-plane renderer. It projects heights from live hit distances, selects caller-supplied VSWAP wall/door pages by the emitted page index, and renders strips into the existing indexed viewport. The headless test now proves the camera renderer observes door descriptors, not just placeholder solid tiles: a locked opening door view hashes to `0x9102177d`, while clearing that door lets the same rays continue to the later wall and hashes to `0x32a9148e`.


## Cycle update: door-aware runtime scene rendering

Added `wl_render_runtime_door_camera_scene_view`, which extends live door-aware camera wall rendering into wall+sprite composition. The helper reuses runtime door-aware wall hits to populate the occlusion-height buffer, projects/sorts sprite surfaces, and composites transparent sprites against live wall/door state. Tests place a sprite behind a locked opening door and assert distinct closed/open canvas hashes (`0x01053e89` and `0xa06c2183`), proving live door descriptors now affect both wall rendering and sprite occlusion in the combined scene path.


## Cycle update: pushwall render descriptors

Added `wl_build_pushwall_wall_hit`, a renderer-facing descriptor seam for moving pushwalls. The helper preserves the source wall id from the low six bits of the original `0xc0` moving marker, selects horizontal/vertical wall pages, and mirrors `WL_DRAW.C::HitHorizPWall` / `HitVertPWall` texture-column flipping based on ray step direction. `wl_cast_runtime_fixed_wall_ray` now routes moving pushwall markers through this seam. Tests assert horizontal and vertical pushwall page/texture descriptors, invalid inputs, and the runtime ray path still emits the expected moving-wall page/texture metadata.


## Cycle update: live pushwall scene occlusion

Extended headless runtime-scene coverage so moving pushwall markers participate in wall+sprite occlusion, not just ray metadata. The test feeds page `73` from local VSWAP data for a `37 | 0xc0` moving marker, renders a sprite behind the pushwall through `wl_render_runtime_door_camera_scene_view`, then clears the marker and verifies a different visible-through scene hash (`0x81e9da6b` blocked vs `0xf80cfa3f` open). This proves live pushwall `tilemap` state is reaching the same door-aware wall+sprite renderer intended to back future SDL3 presentation.


## Cycle update: pushwall sub-tile render offsets

`wl_cast_runtime_fixed_wall_ray` now applies moving-pushwall `pwallpos` to ray distance for live pushwall hits, mirroring the original `HitHorizPWall` / `HitVertPWall` adjustment of the hit intercept before `CalcHeight()`. The texture column remains derived from the original intercept; the sub-tile offset affects distance and projected height. Tests assert a direct east pushwall ray moving from distance `0x8000` to `0x0c000` at `pwallpos=16`, plus a live scene case where the same moving marker changes from hash `0x81e9da6b` to shifted hash `0x83a0d93b` before clearing to `0xf80cfa3f`.


## Cycle update: live runtime-ref scene rendering

Broadened runtime-scene coverage by feeding five real WL6 map-0 `wl_collect_scene_sprite_refs` outputs through the mutable-model door-aware scene renderer. The test decodes the required VSWAP sprite surfaces locally, renders them with `wl_render_runtime_door_camera_scene_view`, asserts the live door center at `(32,57)` emits door page `99`, then clears that tile and confirms the same rays continue to the later wall page `15`. Scene hashes distinguish closed-door (`0x21495346`) and open-door (`0x2e4660d2`) live runtime-ref composition without committing decoded asset bytes.


## Cycle update: headless live gameplay tick

Added `wl_step_live_tick`, a deterministic orchestration seam that ties together existing player motion, visible pickup probing, optional use-button dispatch, door stepping, pushwall stepping, and palette-shift updates. This is not a full game loop yet, but it gives future SDL3 input/render frames one pure-C state transition entry point. Headless tests verify a movement tick can pick up food, heal the player, deactivate the runtime static, and emit a white palette shift; a separate use tick starts a pushwall and advances it across the first block boundary in the same call.


## Cycle update: live tick palette upload

The live tick seam now has coverage proving its palette result can drive renderer upload metadata. The test runs a food-pickup tick, observes the white palette-shift output, and passes that result to the existing shifted texture upload descriptor path. This connects runtime gameplay events to future presentation-facing palette selection without SDL or committed pixel assets.


## Cycle update: live tick scene rendering

Connected the headless live tick seam to live door-aware scene rendering. The test runs a keyed use-button tick against a closed door, lets `wl_step_doors` open and clear the live tilemap entry during the same update, then renders the scene through `wl_render_runtime_door_camera_scene_view` and confirms it matches the independently rendered open-door scene hash. This proves tick-driven mutable state is now on the presentation path.


## Cycle update: live tick pushwall scene rendering

Extended the live tick → scene path from doors to pushwalls. The new test runs a `wl_step_live_tick` use-button update against a pushwall marker, advances the pushwall across the first block boundary in the same tick, verifies the mutable tilemap now carries the `37 | 0xc0` moving marker, and renders that state through `wl_render_runtime_door_camera_scene_view`. The resulting scene hash matches the existing deterministic moving-pushwall occlusion hash, proving tick-driven pushwall state reaches the renderer path.


## Cycle update: live tick static scene removal

Extended the live tick → scene path from doors and pushwalls to pickup statics. The new test builds a small live model with an active food static, collects its renderer-facing sprite ref, decodes the local VSWAP sprite surface, and renders the active static in a door-aware scene. A `wl_step_live_tick` movement update then picks up the static, starts the white palette shift, deactivates the static descriptor, and a fresh `wl_collect_scene_sprite_refs` call returns zero refs before rendering the empty-static scene. Active vs picked scene hashes (`0x7e68266c` vs `0xc928b202`) prove tick-driven static removal reaches renderer-facing scene input.


## Cycle update: actor bite damage seam

Added a first actor/player interaction event seam for dog bites. `wl_try_actor_bite_player` accepts runtime actor descriptors and player motion state, checks the original `T_Bite` proximity window (`abs(delta) - TILEGLOBAL <= MINACTORDIST` on both axes), applies the original chance threshold shape (`chance_roll < 180`), derives damage from `damage_roll >> 4`, and routes successful hits through `wl_apply_player_damage`. Headless tests cover in-range damage, out-of-range no-op, chance miss, baby-difficulty damage scaling, red palette-shift output, and rejection of non-dog actors.


## Cycle update: actor shooting damage seam

Added a deterministic actor shooting event seam following the original `T_Shoot` structure. `wl_try_actor_shoot_player` validates shoot-capable runtime actors, gates on active area and line of sight, computes max tile distance from actor/player tiles, applies the original SS/boss two-thirds distance advantage, derives hit chance from player running and actor visibility, and maps hit damage to close/medium/far `damage_roll` shifts before calling `wl_apply_player_damage`. Headless tests cover inactive area, blocked line of sight, chance miss, far guard damage, SS adjusted-distance medium damage with baby scaling, close guard damage, and non-shootable rejection.


## Cycle update: projectile damage seam

Added a deterministic projectile movement/collision/damage seam based on original `ProjectileTryMove` / `T_Projectile`. `wl_step_projectile` advances a projectile in 16.16 world space, applies the original positive one-tile movement clamp shape, checks `PROJSIZE` collision against the mutable runtime tilemap, detects player impacts with `PROJECTILESIZE`, maps needle/rocket/hrocket/spark/fire damage from the original `damage_roll >> 3` formulas, and routes hits through `wl_apply_player_damage`. Headless tests cover wall impact/removal, needle damage, rocket baby-difficulty scaling, fire projectile travel before impact, tile updates, and inactive-projectile rejection.


## Cycle update: live projectile tick seam

Connected projectile damage into a broader live tick path. `wl_step_live_projectile_tick` mirrors the existing live tick sequence for player motion/use/doors/pushwalls, optionally steps an active projectile with `wl_step_projectile`, then advances palette shifts after projectile damage has updated gameplay state. The headless test verifies a needle projectile impact during the tick damages the player, removes the projectile, and returns a red palette result in the same frame, while a null projectile path leaves projectile stepping disabled.


## Cycle update: live actor attack tick seam

Connected actor bite/shoot damage into live tick orchestration. `wl_step_live_actor_tick` mirrors the existing movement/use/door/pushwall frame seam, dispatches dogs through `wl_try_actor_bite_player` and shootable enemies through `wl_try_actor_shoot_player`, and advances palette state after damage has updated gameplay state. Headless tests cover a dog bite, a guard shot, and a null-actor no-op path with deterministic chance/damage rolls and same-frame red palette results.


## Cycle update: live combat tick seam

Combined the live actor and projectile damage paths into `wl_step_live_combat_tick`. The seam runs movement/use/door/pushwall state once, then applies an optional actor bite/shoot attack and an optional projectile step before a single palette update. Headless tests verify cumulative dog-bite plus needle-projectile damage, projectile removal, red palette selection from the summed damage counter, and a null-combat no-op path.


## Cycle update: live combat upload seam

The combined live combat tick now has direct coverage through the renderer-facing palette upload seam. The test applies same-frame dog-bite and needle-projectile damage, observes the cumulative red palette result, then describes the shifted texture upload and RGBA expansion without SDL or committed asset bytes.


## Cycle update: actor damage state seam

Added a deterministic actor damage/kill state seam outside the original source. `wl_init_actor_combat_state` gives runtime actor descriptors original-style difficulty hitpoints, while `wl_apply_actor_damage` mirrors `DamageActor`/`KillActor` state effects that matter for gameplay integration: doubled damage before attack mode, attack-mode start on nonlethal hits, kill score awards, shootable/alive clearing, and representative item drops. Tests cover guard, SS, boss, and dead-guard cases headlessly.


## Cycle update: actor drop static seam

Actor kill drops now have a runtime model bridge. `wl_spawn_actor_drop_static` consumes `wl_actor_damage_result` drop metadata and appends an active bonus `wl_static_desc` at the actor tile, using the same static type/source metadata that `wl_try_pickup_static_bonus` understands. Tests verify guard clip drops can be picked up for ammo, SS machinegun drops spawn with machinegun static metadata, and no-drop dog kills leave the static list unchanged.


## Cycle update: live actor damage/drop tick

Actor kill drops now flow through live tick orchestration, not just isolated helper calls. `wl_step_live_actor_damage_tick` runs player motion/use, door progression, and pushwall progression, then applies deterministic actor damage and spawns any killed-actor drop into the mutable `wl_game_model::statics` list before palette advancement. The headless test verifies guard kill score, actor alive clearing, spawned static count/index/type/tile metadata, and a null-actor no-op path without touching SDL or proprietary bytes.


## Cycle update: live actor drop scene rendering

Actor drops spawned during a live damage tick now feed renderer-facing scene input. The test kills a guard with `wl_step_live_actor_damage_tick`, confirms the clip drop was appended to `wl_game_model::statics`, collects it as a static scene ref, decodes the referenced local VSWAP sprite into a temporary cache, and renders it through `wl_render_runtime_door_camera_scene_view`. The stable scene hash `0x707dbe4e` proves the tick-spawned drop reaches wall+sprite composition without storing proprietary pixel bytes.


## Cycle update: full live combat tick

The live frame seam now has a fuller combat orchestration path. `wl_step_live_full_combat_tick` mutates the runtime model once for motion/use/doors/pushwalls, then applies outgoing actor damage/drop spawning and incoming actor/projectile damage before one palette update. The headless test verifies killed-guard drop static metadata, projectile removal, actor alive clearing, cumulative player health damage, and no-op behavior when all optional combat inputs are absent.


## Cycle update: actor death-state progression

Actor kill state now has deterministic death animation metadata that can later feed scene refs. `wl_start_actor_death_state` consumes a killed `wl_actor_combat_state` plus `wl_actor_damage_result`, then `wl_step_actor_death_state` advances through original-style death sprite/tic stages. Tests cover guard, dog, and boss sequences, final dead frames, and the one-shot death-scream marker without coupling to audio or SDL.


## Cycle update: actor death-state scene rendering

Actor death animation metadata now feeds renderer-facing scene input. `wl_build_actor_death_scene_ref` turns a killed actor plus `wl_actor_death_state` into a `wl_scene_sprite_ref` with source sprite, VSWAP chunk, and world-position metadata. The test renders a guard death frame through the live door-aware wall+sprite scene path and asserts scene hash `0x2e8b4819` without storing decoded sprite bytes.


## Cycle update: full live combat death refs

The full live combat frame transition now emits renderer-facing death metadata. A killed target actor in `wl_step_live_full_combat_tick` starts death-state progression and builds a `wl_scene_sprite_ref` for the initial death frame before the same result returns drop/static, projectile, actor attack, and palette data. Tests assert guard death source/chunk metadata (`91` / `197`) and no-op flags for null combat.


## Cycle update: full combat death-ref scene rendering

Full live combat output now feeds the live scene renderer directly for death frames. The test kills a guard with `wl_step_live_full_combat_tick`, consumes the returned `actor_death_ref`, decodes its local VSWAP sprite surface, and renders it through `wl_render_runtime_door_camera_scene_view`. The hash matches the standalone death-state render (`0x2e8b4819`), proving the frame result can drive presentation without an intermediate hand-built ref.


## Cycle update: actor death final-frame refs

Runtime actor refs now have a small death-finalization seam. A finished `wl_actor_death_state` can be applied to a model actor slot, marking it inert/non-shootable and replacing the source sprite emitted by `wl_collect_scene_sprite_refs` while preserving the actor model index and world position. The headless guard test verifies source `50` becomes final source `95` / chunk `201`.


## Cycle update: live actor death tick

Actor death animation now advances through a runtime tick seam. `wl_step_live_actor_death_tick` steps death-state tics, emits a scene ref for the current death frame, and applies the final corpse-frame override to the model actor slot when finished. The guard test proves the same actor model index progresses to source `92`, then persists as source `95` / chunk `201` through normal scene-ref collection.


## Cycle update: full combat death tick orchestration

Full live combat can now advance active actor death animation in the same frame boundary. `wl_step_live_full_combat_death_tick` returns the normal full-combat transition plus optional death-tick output, so a previously killed guard can progress to its final corpse-frame override while the mutable model still carries spawned drops and ordinary scene refs.


## Cycle update: full combat death-tick final scene

The combined full-combat/death-tick frame result now reaches the renderer. A guard killed by full combat is advanced to final death state, its returned source `95` / chunk `201` ref is decoded locally, and `wl_render_runtime_door_camera_scene_view` produces stable hash `0x81c10dcf`. This proves the combined frame boundary can drive final corpse presentation without a hand-built scene ref.


## Cycle update: broadened actor death final refs

Final-frame actor ref replacement now covers representative non-guard enemy classes. Finished death states for officer, SS, dog, mutant, and boss are applied to runtime actor slots and collected through the normal scene-ref path, preserving model index/world position while changing source/VSWAP metadata to the final corpse frame.


## Cycle update: patrol path direction seam

Added the first deterministic patrol-path selection helper. `wl_select_path_direction` consumes runtime path markers plus the mutable tilemap to mirror the `SelectPathDir` shape before full `T_Path`: markers can redirect patrol actors, blocked next tiles yield `WL_DIR_NONE`, and diagonal arrow markers remain metadata-only until diagonal movement is modeled.


## Cycle update: patrol actor step seam

The patrol path-direction seam now mutates runtime actor descriptors through `wl_step_patrol_actor`. A patrol actor consumes marker-selected/current direction, advances one tile on clear paths, preserves position on blocked paths, and reports deterministic step/block metadata for future `T_Path` tic-distance integration.


## Cycle update: patrol actor scene refs

Patrol actor movement now reaches the normal runtime scene-ref collection path. After `wl_step_patrol_actor` mutates a patrol actor slot, `wl_collect_scene_sprite_refs` emits the updated world-center coordinates while preserving the patrol guard sprite source/VSWAP metadata; blocked steps preserve the previous renderer-facing position.


## Cycle update: patrol actor scene render

Patrol actor refs now flow into the door-aware runtime scene renderer. The test feeds patrol guard source/VSWAP/world metadata through `wl_render_runtime_door_camera_scene_view`, verifies projection visibility/source metadata, and preserves a deterministic canvas hash without storing decoded sprite bytes.


## Cycle update: patrol actor tic stepping

Added a tile-granular `T_Path`-style budget loop. `wl_step_patrol_actor_tics` computes `speed * tics`, repeatedly consumes full-tile moves via `wl_step_patrol_actor`, reports leftover partial movement, and preserves actor position when the next full tile is blocked.


## Cycle update: patrol actors batch tick

Added a small multi-actor patrol tick seam. `wl_step_patrol_actors_tics` scans the runtime actor list, applies the tic-budgeted patrol step only to patrol-mode actors, skips standing actors, and reports aggregate considered/stepped/blocked/tile counts for future live AI orchestration.


## Cycle update: live actor AI patrol tick

Patrol actor ticking now has a broader live-frame wrapper. `wl_step_live_actor_ai_tick` runs the existing live tick sequence, then applies aggregate patrol movement and returns both live environment/palette output and patrol AI counts, keeping actor AI mutation ready for renderer-facing collection.


## Cycle update: live AI patrol scene refs

Live actor AI output now reaches normal runtime scene-ref collection. The headless test advances a patrol actor through `wl_step_live_actor_ai_tick`, then verifies `wl_collect_scene_sprite_refs` reports source/chunk `58/164` at the moved world coordinates while a later blocked live-AI step preserves that renderer-facing position.


## Cycle update: live AI patrol rendering

Live actor AI patrol movement now reaches runtime rendering. A patrol guard moved by `wl_step_live_actor_ai_tick` is collected as a scene ref and rendered through the door-aware scene path with source `58` / chunk `164`, visible projection metadata, and canvas hash `0x6ee1f8bf`.


## Cycle update: patrol fine-position refs

Patrol movement now preserves fixed-point actor placement. `wl_actor_desc` carries optional `fine_x`/`fine_y`, `wl_step_patrol_actor_tics` stores partial movement from leftover tic budget, and scene-ref collection emits fine coordinates when present while preserving whole-tile fallback behavior.


## Cycle update: live AI fine-position patrol rendering

The live actor AI frame wrapper now preserves fine-position patrol output all the way to runtime scene refs and rendering. A half-tile budget keeps the guard on tile `(5,5)` but emits world `0x60000/0x58000`, then renders through the door-aware scene path with hash `0xcf61b07b`.


## Cycle update: patrol remainder accumulation

`wl_actor_desc` now carries `patrol_remainder` so tic-budgeted patrol motion accumulates across frames. A half-tile update advances fine refs, a second half-tile update consumes the accumulated whole tile, and scene-ref fallback remains deterministic.


## Cycle update: live AI remainder rendering

Multi-frame patrol remainder is now covered at the live AI frame/render boundary. Two half-tile `wl_step_live_actor_ai_tick` calls first emit fine refs, then consume one full tile and clear `patrol_remainder`, proving accumulated movement reaches scene refs and rendering.


## Cycle update: live AI dog fine-position rendering

Live AI fine-position refs now cover dog patrol actors as well as guards. The dog remains on its tile with a half-tile budget, emits source/chunk `99/205` at fine world coordinates, and renders through the same door-aware scene path.


## Cycle update: live AI officer fine-position rendering

Live AI fine-position refs now cover officer patrol actors in addition to guards/dogs. A half-tile officer patrol emits source/chunk `246/352` at fine world coordinates and renders through the door-aware scene path.


## Cycle update: live AI SS fine-position rendering

Live AI fine-position refs now cover SS patrol actors alongside guards/dogs/officers. A half-tile SS patrol emits source/chunk `146/252` at fine world coordinates and renders through the door-aware scene path.


## Cycle update: live AI mutant fine-position rendering

Live AI fine-position refs now cover mutant patrol actors alongside guards/dogs/officers/SS. A half-tile mutant patrol emits source/chunk `195/301` at fine world coordinates and renders through the door-aware scene path.


## Cycle update: live AI boss fine-position rendering

Live AI fine-position refs now cover a boss actor alongside normal patrol classes. A half-tile Hans boss actor emits source/chunk `296/402` at fine world coordinates and renders through the door-aware scene path.


## Cycle update: chase direction selection seam

Runtime actor AI now has a headless `SelectChaseDir`-style direction seam. `wl_select_chase_direction` prefers the player-facing axis, avoids immediate turnaround, falls back through current/search/turnaround directions, and reports the next tile or blocked state.


## Cycle update: chase actor step seam

`wl_step_chase_actor` connects `wl_select_chase_direction` to actor runtime state. Chase actors now update direction, tile position, fine coordinates, blocked state, and scene refs deterministically.


## Cycle update: chase tic fine-position seam

`wl_step_chase_actor_tics` gives chase actors the same deterministic sub-tile movement bridge as patrol actors. Partial movement stores fine coordinates/remainder, and the next frame consumes the carried movement into a whole tile.


## Cycle update: chase fine-position rendering

Chase tic fine-position movement now reaches scene refs and the door-aware renderer. A half-tile chase guard remains on its tile, emits fine coordinates, and renders deterministically through the same path used by live AI patrol refs.


## Cycle update: live AI chase fine-position rendering

`wl_step_live_actor_ai_tick` now aggregates chase actors through `wl_step_chase_actors_tics` after the normal live tick and patrol pass. The live AI result reports chase counts while scene refs preserve fine-position chase coordinates.


## Cycle update: live AI chase remainder rendering

Multi-frame chase remainder now reaches the live AI render boundary. Two half-tile live AI frames first emit fine chase refs, then consume the accumulated full tile and clear actor remainder.


## Cycle update: live AI chase combat bridge

Chase movement now connects to the existing actor attack tick seam. After live AI consumes chase remainder into a tile-centered actor position, the moved runtime actor can drive `wl_step_live_actor_tick` shooting metadata and damage.


## Cycle update: live AI dog chase bite bridge

The live AI chase/combat bridge now covers contact attackers as well as shooters. A dog chase actor completes accumulated movement before `wl_step_live_actor_tick` applies bite damage from the moved tile.


## Cycle update: live AI shooter-class chase combat

The live AI chase/combat bridge now covers the shooter class matrix. Officer, SS, mutant, and boss actors complete accumulated chase movement before feeding `wl_step_live_actor_tick`, including SS/boss distance scaling.


## Cycle update: live AI chase full-combat bridge

Live AI chase movement now feeds the full-combat seam. After accumulated chase movement completes, the moved guard participates in one frame that applies player shot damage, actor damage, drop spawning, and death-ref construction.


## Cycle update: live AI chase full-combat render

The chase full-combat result now feeds scene refs/rendering. The moved guard death ref and spawned drop ref render together, proving the runtime model carries chase-started combat output to presentation metadata.


## Cycle update: live AI chase death-final render

The live AI chase/full-combat path now flows through `wl_step_live_full_combat_death_tick`. The actor slot receives the final corpse override after death ticking, while the drop remains collectible scene state.


## Cycle update: live AI chase death-transition render

The chase death render case now records both initial-death and final-corpse scene hashes. This pins the multi-frame runtime transition after chase full combat instead of only validating the terminal frame.


## Cycle update: shooter-class chase death-final metadata

The chase full-combat/death-final seam now covers officer, SS, mutant, and boss classes. Each case validates final corpse scene-source override after chase movement and full-combat death ticking.


## Cycle update: shooter-class chase death-final render hashes

The class-broadened chase death-final metadata now reaches the headless scene renderer. Officer, SS, mutant, and boss cases each render final corpse plus drop after live AI chase movement and death ticking.


## Cycle update: dog chase death-final render

The chase full-combat/death-final render seam now covers dog actors separately from shooter/drop classes. The dog final corpse override renders without a spawned drop.


## Cycle update: headless presentation frame descriptor

A rendered live-AI dog chase death frame now feeds `wl_describe_present_frame`, producing deterministic upload/viewport/hash metadata for a future SDL3 presentation layer.


## Cycle update: palette-shifted present frame descriptor

The present-frame seam now carries palette-shift metadata for a rendered live-AI frame, proving future SDL3 upload code can distinguish base and damage-flash palettes without changing indexed pixels.


## Cycle update: combat palette present frame descriptors

The SDL-free presentation seam now verifies both damage/red and bonus/white palette shifts on a rendered live-AI frame, keeping indexed pixels stable while changing upload palette metadata.


## Cycle update: live combat present-frame descriptor

A real `wl_step_live_combat_tick` red damage palette now reaches the SDL-free present-frame descriptor, connecting gameplay damage state to future texture-upload metadata.


## Cycle update: chase attack scene present descriptor

Live chase movement, shooter attack damage, scene rendering, and present-frame metadata now meet in one deterministic headless path. The chase attack render hash `0x4a4c3e4f` is described with the attack-produced red palette shift.

## Cycle update: player weapon fire/ammo seam

Added `wl_try_player_fire_weapon`, a small deterministic attack-state helper for future live player combat. The seam validates requested weapon availability, consumes one ammo for pistol/machinegun/chaingun shots, leaves knife attacks ammo-free, records no-ammo fallback to knife, and reports unavailable weapon requests without consuming ammo. Headless tests cover chaingun ammo consumption, knife fire, empty-pistol fallback, unavailable chaingun requests, and invalid weapon rejection.

## Cycle update: live player fire tick bridge

Added `wl_step_live_player_fire_tick`, a deterministic live-frame wrapper that combines player motion/use/door/pushwall/palette ticking with the weapon fire/ammo seam. The result reports normal live tick metadata plus an optional fire result, covering fired machinegun ammo consumption, empty-pistol fallback to knife, and no-fire frames without requiring SDL.

## Cycle update: player attack-frame tick seam

Added `wl_step_player_attack_state()` as a deterministic SDL-free bridge for player attack state progression. The helper advances `attack_frame` by elapsed tics, reports frame/weapon before and after, clamps completed attacks to frame 0, and restores the chosen non-knife weapon when ammo is available after a temporary knife fallback. `wl_step_live_tick()` now includes this attack-step descriptor alongside motion/use/door/pushwall/palette outputs so future input and combat loops can carry player attack animation state without SDL.

Headless coverage verifies partial advancement, completion with chosen-weapon restoration, empty-ammo completion staying on knife, invalid negative tics rejection, and live-tick attack descriptor propagation.

## Cycle update: player fire starts attack frame seam

Added `wl_try_player_fire_weapon_attack()` as a deterministic bridge between the player fire/ammo decision and attack-frame state. The helper wraps `wl_try_player_fire_weapon()`, starts `attack_frame` only when a shot/knife attack actually fires, leaves no-ammo fallback frames unchanged, and reports before/after frame metadata for future live player-combat orchestration. Headless coverage pins fired, no-ammo, and invalid attack-duration paths.

## Cycle update: player fire attack-start seam

Added `wl_start_player_fire_attack()` as an SDL-free helper that combines weapon fire/ammo validation with deterministic attack-frame startup metadata. Accepted knife/pistol/machinegun/chaingun shots now report the previous attack frame and initialize a small weapon-specific attack duration, while no-ammo fallback and unavailable weapons leave the existing attack frame unchanged. Headless tests cover chaingun startup/ammo consumption, empty-pistol fallback, unavailable weapon requests, and invalid weapon rejection.

## Cycle update: live player fire attack-frame integration

`wl_step_live_player_fire_tick()` now routes fire-button input through `wl_start_player_fire_attack()` instead of the ammo-only seam, preserving the existing `fire` compatibility metadata while also reporting `fire_attack` frame-start metadata. This keeps the live SDL-free tick path aligned with the player attack-state seam: a successful shot consumes ammo and starts the weapon-specific attack frame in the same tick, no-ammo fallback preserves the current frame after the regular live tick advance without starting a new attack frame, active attack frames block same-tick refire without consuming ammo even on the tick where the prior attack finishes, and idle ticks leave fire/attack-start descriptors empty. The live result exposes `fire_blocked_by_active_attack` so future input/audio/presentation glue can distinguish a pressed-but-suppressed fire button from an idle tick. Headless coverage pins fired, fallback, active-frame block, finish-tick block, and no-fire cases.

## Cycle update: WL6 runtime info-tile classification sweep

The WL6 model harness now sweeps all 60 gameplay map slots and asserts that every map which can currently build a runtime model has zero unknown info-plane tiles. This pins boss/ghost/static/path classifications across the full WL6 map set, including boss source tiles 160/178/179/196/197/214/215, while documenting the two legacy maps that still fail runtime model construction before classification can be checked.

## Cycle update: aggregate actor wake transition

Added `wl_wake_actors_for_chase()` as a deterministic batch helper for alerting eligible shootable actors into chase mode. The helper can exclude ambush actors for sight/sound-sensitive wake paths or include them for explicit all-alert events, skips inert/dead/invalid actors, preserves already-chasing actors, and reports considered/woken/ambush-cleared/direction-selected counts plus first/last woken actor indexes for future live AI orchestration diagnostics.

## Cycle update: live actor wake AI tick

Added `wl_step_live_actor_ai_wake_tick()` as an opt-in live AI orchestration seam that runs the normal live player tick, wakes eligible shootable actors at the current player tile, then advances patrol/chase actor movement. The existing `wl_step_live_actor_ai_tick()` remains wake-free for tests that need isolated patrol/chase progression, while both paths share the same implementation and return wake diagnostics (`actors_woke`, counts, and first/last woken actor indexes). Headless coverage verifies non-ambush wake, later ambush-inclusive wake, and sentinel indexes when no additional actors wake.

## Cycle update: actor mode summaries

Added `wl_count_actors_by_mode` as a small runtime model summary helper alongside `wl_count_actors_by_kind`. The broader WL6 boss/secret/map coverage now validates mode histograms against model actor descriptors and covers undersized output rejection, keeping actor state-progression summaries deterministic before deeper AI/live-frame integration.

Added `wl_summarize_actor_flags` as the flag-oriented sibling to the actor kind/mode summary helpers. The WL6 broader map sweep now verifies shootable, ambush, kill-total, scene-override, and inert counts from built actor descriptors, including `kill_total_count == model.kill_total`, so future wake/chase/death transitions have a compact deterministic aggregate check.

## Cycle update: actor position summaries

Added `wl_summarize_actor_positions` as a compact runtime actor-position diagnostic. The broader WL6 map coverage now verifies spawn/current tile bounds, initial patrol movement away from spawn, and zeroed fine-position state for freshly built models, giving future chase/patrol interpolation work a deterministic headless aggregate check before mutating actor coordinates.


## Cycle update: actor wake-state summaries

Added `wl_summarize_actor_wake_state` as a compact diagnostic for AI wake eligibility. It partitions actors into immediately wakeable, ambush-waiting, already chasing, and ineligible buckets, with a switch for including ambush actors. The broader WL6 map sweep now cross-checks both ambush-excluded and ambush-included summaries against built actor descriptors, giving future sight/sound wake logic a deterministic headless aggregate check before mutating actor modes.

## Cycle update: actor patrol-path summaries

Added `wl_summarize_actor_patrol_paths` as the patrol-oriented sibling to the existing actor aggregate helpers. It counts patrol actors with a selected path direction, patrol actors blocked/no-directed at their current tile, and patrol actors with invalid positions, letting future AI ticks verify path readiness without mutating actor state. The broader WL6 map sweep cross-checks the summary against `wl_select_path_direction()` for each patrol actor.

## Cycle update: actor attack-role summaries

Added `wl_summarize_actor_attacks` as a compact, non-mutating diagnostic for live actor-combat readiness. It partitions valid-position actors into dog bite attackers, shoot-capable actors matching the live combat `actor_can_shoot` kind set, passive/non-attacking actors, and invalid-position actors. The broader WL6 map sweep cross-checks those buckets against each actor descriptor, and a synthetic case pins shoot/bite/invalid handling before deeper live AI attack orchestration mutates actor state.

## Cycle update: actor scene-source summaries

Added `wl_summarize_actor_scene_sources` as a compact renderer-readiness diagnostic for runtime actor descriptors. It partitions actors into default scene-source mappings, explicit override mappings, and missing scene-source metadata without mutating the model. The broader WL6 map sweep cross-checks the summary against actor descriptors, and synthetic coverage pins default/override/missing buckets before deeper actor render-state transitions.

## Cycle update: actor source-tile summaries

Added `wl_summarize_actor_source_tiles` to report actor source-tile coverage (total actors, unique source tiles, zero-source placeholders, and min/max source tile) as a compact non-mutating diagnostic for checking model construction against map semantics before renderer or AI code consumes actor descriptors. Synthetic headless coverage pins duplicate/zero/min/max behavior and null-argument rejection.

## Cycle update: actor direction summaries

Added `wl_summarize_actor_directions` as a tiny non-mutating actor-state diagnostic. It reports counts for north/east/south/west/none actor directions and separately counts invalid direction values, giving future patrol/chase/render transitions a compact sanity check before mutating direction state.

## Cycle update: actor engagement state flags

The actor engagement summary now separates active chase/boss/ghost-mode threats from ambush-flagged threats. This keeps the gameplay/render orchestration seam aware of whether nearby melee/ranged actors are already active or still marked as ambush while remaining SDL-free and test-backed.

## Cycle update: actor engagement distance range

Extended `wl_actor_engagement_summary` with the farthest active threat index and Manhattan tile distance, complementing the existing nearest-threat fields. The SDL-free synthetic coverage now pins nearest/farthest threat selection and the no-threat sentinel reset, giving future AI/audio/render orchestration a compact threat-distance range without mutating actor state.

- Added a headless actor motion summary seam that partitions centered actors, sub-tile offset actors, active movement remainders, invalid actor positions, farthest sub-tile offset actor/index, and active remainder min/max ranges for runtime state progression diagnostics.
## Cycle update: actor activity summary

Added `wl_summarize_actor_activity()` as a compact SDL-free diagnostic for live actor-state progression. The helper partitions valid actors into active AI, waiting AI, inert, combat-ready, and boss/ghost buckets while reporting invalid positions, giving future gameplay/render orchestration a stable summary before deeper per-state ticks.

## Cycle update: actor tile occupancy summary

Added `wl_summarize_actor_tile_occupancy()` as a non-mutating runtime actor-state diagnostic. It counts occupied actor tiles, actors sharing stacked tiles, invalid positions, and the maximum stack depth so future collision/AI progression can detect overlapping actor states headlessly before mutating live actors.

## Cycle update: actor spawn occupancy summary

Added `wl_summarize_actor_spawn_occupancy()` as the spawn-tile sibling to live actor tile occupancy. It reports occupied spawn tiles, actors sharing the same original spawn tile, invalid spawn coordinates, moved-from-spawn actors, and maximum spawn stack depth so model diagnostics can distinguish original placement overlap from later live movement.

## Cycle update: actor collision-tile summary

Added `wl_summarize_actor_collision_tiles()` as a small non-mutating actor placement diagnostic. It partitions valid actor positions by the runtime tilemap category under the actor (open, wall, door, door-adjacent marker) and reports invalid positions, giving future live collision/AI work a deterministic headless check for actors embedded in blocking runtime map state.

## Cycle update: actor/player adjacency summary

Added `wl_summarize_actor_player_adjacency()` as a small non-mutating runtime actor-state diagnostic. It partitions actors by same-tile, cardinal-adjacent, diagonal-adjacent, same-row/column, distant, and invalid-position buckets relative to a supplied player tile, giving future bite/chase/contact-damage ticks a deterministic headless proximity check before mutating actor AI state.

## Cycle update: actor door-proximity summary

Added `wl_summarize_actor_door_proximity()` as a small non-mutating actor placement diagnostic. It partitions actors standing on runtime door tiles, actors on door-adjacent marker tiles, actors away from doors, and invalid positions, while reporting the unique door indices touched by actors. This gives future door-blocking, AI navigation, and close-door collision work a deterministic headless aggregate before mutating live door or actor state.

## Cycle update: actor-facing summary

Added `wl_summarize_actor_facing()` as a non-mutating actor/player-facing diagnostic for runtime AI/chase characterization. It buckets actors into facing-player, facing-away, perpendicular, same-tile, no-direction, invalid-direction, and invalid-position counts relative to a supplied player tile, giving future chase/contact behavior a deterministic SDL-free summary before mutating actor state.

## Cycle update: actor combat-class summary

Added `wl_summarize_actor_combat_classes()` as a compact non-mutating actor descriptor diagnostic. It groups actors into infantry, dog, boss, ghost, corpse, invalid-kind, shootable, kill-credit, and noncombat buckets so future combat/death/drop orchestration can sanity-check class metadata before mutating live actor state. Synthetic headless coverage pins each bucket and null-argument rejection.

## Cycle update: actor threat summary

Added `wl_summarize_actor_threats()` as a compact non-mutating actor-state diagnostic. It partitions shootable actors into immediate active threats, latent stand/patrol threats, ambush latent threats, and inert shootable actors while separately counting non-shootable actors. Synthetic headless coverage pins each bucket before deeper live AI/combat orchestration mutates actor state.

## Cycle update: runtime static state summary

Added `wl_summarize_static_states`, a non-mutating runtime static diagnostic for live pickup/render orchestration. It reports active vs inactive statics, blocking statics, bonus statics, treasure statics, and the active subsets that still affect collision or pickup/render paths. Synthetic headless tests cover active/inactive blocking and bonus/treasure buckets plus null-argument rejection.

## Cycle update: runtime static source-tile summary

Added `wl_summarize_static_source_tiles`, a compact static-classification diagnostic matching the existing actor source-tile summary shape. It reports total statics, unique source-tile ids, zero-source placeholders, and min/max source tiles without mutating runtime state, giving SOD/WL6 static classification and scene-ref work a cheap pre-render sanity check. Synthetic headless tests cover duplicates, zero source tiles, empty models, and null-argument rejection.

Added `wl_summarize_static_tile_occupancy`, mirroring the actor tile-occupancy diagnostic for static descriptors. It reports occupied static tiles, stacked static descriptors, invalid coordinates, and maximum stack depth so pickup/render collision seams can spot overlapping statics before mutating active state. Synthetic headless coverage pins stacked, singleton, invalid, and empty-model behavior.

## Cycle update: runtime static/player distance summary

Added `wl_summarize_static_player_distances`, a non-mutating static-object diagnostic that mirrors the actor/player distance seam for pickup and render preparation. It reports nearest/farthest static descriptors by tile Manhattan distance, invalid static coordinates, and inactive statics skipped when callers request active-only coverage. Synthetic headless tests cover all result fields, null/invalid-player rejection, active-only filtering, and the no-considered-static sentinel case.

## Cycle update: runtime door state summary

Added `wl_summarize_door_states()` as a compact non-mutating diagnostic for live door progression. It partitions door descriptors by orientation, lock state, action state, moving/partial-open state, invalid action values, and maximum door position, giving future collision/presentation orchestration a deterministic headless sanity check before mutating door state.

## Cycle update: runtime door timing summary

Added `wl_summarize_door_timing()` as a companion non-mutating diagnostic for live door ticcounts. It buckets waiting, positive countdown, overdue, moving-with-countdown, and open-with-countdown doors while reporting min/max ticcount bounds, giving future door progression and SDL presentation code a deterministic headless timing sanity check.

## Cycle update: runtime pushwall state summary

Added `wl_summarize_pushwall_state()` as a non-mutating diagnostic for pushwall markers and active moving-pushwall metadata. It reports marker direction buckets, invalid marker coordinates, active motion tile/state/pos, and whether the active motion references an in-range marker and map coordinate. Synthetic headless tests cover direction buckets, invalid markers, valid active motion, and stale active-motion references without requiring SDL.

## Cycle update: runtime pushwall/player distance summary

Added `wl_summarize_pushwall_player_distances()` as a non-mutating pushwall proximity diagnostic. It reports nearest and farthest pushwall markers from a supplied player tile plus invalid marker coordinates, with synthetic headless tests covering normal, invalid-input, and no-considered-marker cases for future pushwall collision/presentation orchestration.

`wl_summarize_door_locks()` now provides a small non-mutating lock diagnostic for live door descriptors. It partitions normal, keyed, and elevator/other lock ids, tracks unique/min/max lock values, and cross-checks descriptor locks against original door source tiles `90..101` when present. Synthetic headless tests cover expected lock classes, invalid source tiles, descriptor/source mismatches, null arguments, and an empty door set.

## Cycle update: runtime pushwall source-tile summary

Added `wl_summarize_pushwall_source_tiles()` as a non-mutating diagnostic for pushwall marker provenance. It reports unique marker source tiles, counts the expected original info-plane marker tile `98`, and flags zero or unexpected source tiles with min/max bounds. Synthetic headless tests cover duplicate expected markers, zero/unexpected source tiles, invalid arguments, and empty models.

## Cycle update: runtime static/player adjacency summary

Added `wl_summarize_static_player_adjacency`, a non-mutating static-object proximity diagnostic for pickup/render readiness. It partitions valid statics into same-tile, cardinal-adjacent, diagonal-adjacent, same-row/column, and distant buckets relative to a player tile, while preserving the active-only filter and invalid-coordinate accounting used by the static/player distance helper. Synthetic headless tests cover every bucket plus null, invalid-player, inactive, and invalid-coordinate paths.
