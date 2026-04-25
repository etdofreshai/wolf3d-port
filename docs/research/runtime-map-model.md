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

Added WL6 boss tile handling for the non-Spear boss spawns (`SpawnBoss`, `SpawnSchabbs`, `SpawnGift`, `SpawnFat`, `SpawnFakeHitler`, `SpawnHitler`, and `SpawnGretel`) as data-only runtime actors. The model records their source tile, original north/south starting direction, shootable/kill-total behavior, and renderer-facing starting sprite when known. Headless coverage now includes `Wolf2 Boss`, `Wolf3 Boss`, `Wolf4 Boss`, and `Wolf6 Boss`, asserting map/model totals, zero unknown info tiles, actor-kind counts, scene-ref counts/hashes, and broader boss mixes (mutants, guards, officers, SS, and boss actors). `Wolf1 Boss` and `Wolf5 Boss` are intentionally left for a later classification pass because they still report unknown info tiles in the current model.


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
