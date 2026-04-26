# Modern C + SDL3 Port

Target implementation for the Wolfenstein 3D modern C port using SDL3.

Goals:

- Stay close to original Wolfenstein 3D behavior and structure where practical.
- Replace DOS/platform-specific services with SDL3-backed equivalents.
- Keep game data proprietary assets outside git in `game-files/`.
- Use tests as the behavioral bridge from original source to this implementation.

## Current headless verification

The first runnable milestone is a pure C asset/decompression/semantics/model harness. It has no SDL dependency yet and verifies local WL6 data discovery, `MAPHEAD`, `GAMEMAPS`, `AUDIOHED`/`AUDIOT` sound/music metadata including metadata-backed looped IMF playback positions, `VGAHEAD`/`VGADICT`/`VGAGRAPH` Huffman graphics smoke decoding, `STRUCTPIC` picture-table metadata, planar-to-indexed picture surface conversion with indexed-surface descriptors, upload metadata, palette interpolation/fade/shift metadata, gameplay palette-shift state, and palette-shifted upload selection, RGBA expansion, and SDL-free blitting, full `VSWAP` chunk-directory parsing, bounded chunk reads, wall-page metadata/surface conversion, wall texture-column sampling, fixed-height wall strip scaling, tiny wall-strip viewport composition, map-derived wall-hit descriptors, cardinal/fixed-point/DDA/projected ray stepping, view batches, and camera ray tables, and tiny view rendering, sprite shape metadata, sprite post-command metadata, sprite indexed surfaces, scaled sprite rendering, world-space sprite projection/ordering, combined wall+sprite scene rendering, Carmack/RLEW map-plane decompression, first-pass map semantic classification, a minimal `SetupGameLevel`-style runtime model, and runtime sprite-reference selection, sprite surface-cache decoding, and cached runtime-ref sprites feeding combined wall+sprite scene rendering across representative visible refs/camera angles, and multi-map runtime actor coverage for guards, SS, officers, dogs, mutants, bosses, and ghosts, including expanded WL6 boss/secret-map scene-ref assertions, plus player damage/bonus pickup/score/ammo/weapon/key event seams and runtime static pickup/removal hooks, a TransformTile-style visible pickup probe, and a small player motion/collision tick, and use-button door/elevator/pushwall dispatch metadata, and deterministic door open/close progression, and pushwall start/progression metadata, a live runtime solid-plane raycast/render bridge, door-wall render descriptors, runtime door-aware ray hits, door-aware runtime camera wall rendering, and door-aware runtime wall+sprite scene rendering, pushwall wall-page/texture descriptors, and live pushwall scene occlusion.

Run from this directory:

```bash
make test
```

`make test` always runs the pure-C headless harness, then runs the SDL3 dummy-video smoke/presentation checks when SDL3 is available (otherwise it prints a skip message). Use `make test-headless` when a strict no-SDL verification gate is needed. By default the tests use `game-files/base` relative to the repo root. Override with:

```bash
WOLF3D_DATA_DIR=/path/to/WL6/data make test
```

The test only reads local data files and commits metadata/hash/semantic/model/chunk-count expectations, not proprietary game bytes.


## Runtime wall-view bridge

`wl_build_runtime_solid_plane` converts mutable model `tilemap` state into the existing solid wall-plane raycaster, and `wl_render_runtime_camera_wall_view` feeds that bridge directly into the headless wall-view renderer. Current tests verify that a closed door blocks and renders as a placeholder solid, then clearing that same door lets the camera rays continue to a later wall with a different deterministic canvas hash. Moving pushwall solid markers are also preserved for ray hits.


## Door wall descriptors

`wl_build_door_wall_hit` mirrors the original `HitHorizDoor`/`HitVertDoor` door-page and texture-offset selection without requiring SDL. It derives `DOORWALL = PMSpriteStart - 8`, chooses normal/locked/elevator door pages, applies vertical-side page offsets, and subtracts runtime `doorposition` from the fixed-point intercept before producing a renderer-facing `wl_map_wall_hit`. The headless test renders a locked vertical door strip from local VSWAP pages and asserts a stable canvas hash.


## Runtime door-aware rays

`wl_cast_runtime_fixed_wall_ray` extends the fixed-point DDA ray seam to read live `wl_game_model::tilemap` state directly. It preserves moving pushwall wall ids, skips door-side markers, resolves door-center markers through runtime door descriptors, and emits door wall-page/texture metadata with `doorposition` applied. This lets future camera render code distinguish a real door hit from a placeholder solid wall before SDL3 presentation is introduced.


## Door-aware runtime camera rendering

`wl_render_runtime_door_camera_wall_view` connects live runtime DDA hits to the existing indexed wall-strip renderer. It builds camera ray directions, casts each ray through mutable `wl_game_model::tilemap` with door descriptors and `doorposition` applied, projects wall height from hit distance, selects caller-provided VSWAP wall/door pages, and renders the result into a headless indexed viewport. Current tests compare closed-door and open-door canvas hashes without requiring SDL.


## Door-aware runtime scene rendering

`wl_render_runtime_door_camera_scene_view` extends the live door-aware camera wall path into the combined wall+sprite renderer. It records live wall heights from runtime door/wall hits, projects and sorts sprite surfaces, and composites transparent sprites with the same occlusion buffer used by the existing immutable-map scene renderer. Headless tests now compare closed-door and open-door scene hashes with a sprite behind the doorway, proving live door state affects both wall selection and sprite occlusion before SDL3 presentation.


## Pushwall render descriptors

`wl_build_pushwall_wall_hit` adds a headless descriptor seam for original-style moving pushwalls. It maps the low six bits of the moving `0xc0` tile marker back to the source wall id, selects horizontal/vertical wall pages with the same page-index convention as regular wall hits, and mirrors `HitHorizPWall` / `HitVertPWall` texture-column flipping based on ray step direction. `wl_cast_runtime_fixed_wall_ray` now routes moving pushwall markers through this seam, so live pushwall state emits renderer-facing wall descriptors instead of acting like a generic solid tile.


## Live pushwall scene occlusion

The runtime door-aware scene renderer is now covered with moving pushwall state as well as doors. A headless test supplies the real VSWAP wall page for a moving pushwall tile, renders a sprite behind that wall, then clears the moving marker and verifies a distinct visible-through state. This keeps live `tilemap` pushwall state on the wall+sprite presentation path without introducing SDL3 or committing decoded asset bytes.


## Pushwall sub-tile render offsets

`wl_cast_runtime_fixed_wall_ray` now applies the original moving-pushwall `pwallpos` offset to hit distance before projected wall height is computed. Texture-column selection still follows `HitHorizPWall` / `HitVertPWall`, while the adjusted distance makes the live renderer observe sub-tile pushwall movement. Headless scene coverage now distinguishes the same pushwall at `pwallpos=0`, at `pwallpos=16`, and after the marker is cleared.


## Live runtime-ref scene rendering

The broader WL6 runtime-ref sprite set now also flows through `wl_render_runtime_door_camera_scene_view`, not only the immutable wall-plane scene renderer. A headless test renders five real `wl_collect_scene_sprite_refs` outputs against the mutable `wl_game_model::tilemap`, verifies the closed door at `(32,57)` emits door page `99`, then clears that door center and verifies rays continue to the later wall page. The closed/open live scene hashes are committed as metadata only.


## Headless live gameplay tick

`wl_step_live_tick` is a small deterministic orchestration seam for future frame updates. It combines player motion/pickup probing, optional use-button dispatch, door progression, pushwall progression, and palette-shift advancement into one headless call. Current tests cover a movement tick that picks up food and emits a white palette shift, plus a use tick that starts and advances a pushwall across its first block boundary.

The use-button result now reports keyed-door requirements directly: `required_key` records the zero-based key bit for locked runtime doors, `has_required_key` shows whether the current player state can open it, and `door_action_before`/`door_action_after` expose the door transition. It also captures the targeted runtime tile value as `tile_before`/`tile_after`, so elevator and pushwall uses expose their immediate tilemap mutation while door uses document that the door-center marker remains stable. Pushwall uses also report `pushwall_active_before`/`pushwall_active_after`, distinguishing a newly started secret wall from a use attempt suppressed by an already moving pushwall. Door/elevator targets record `button_held` and `suppressed_by_button_hold` so live input repeat suppression can be presented without re-running use dispatch. The result also snapshots the target plane words as `wall_plane_value`/`info_plane_value`, preserving source-map context for no-op, elevator, door, and pushwall presentations. This keeps future HUD/status feedback and SDL3 input presentation from re-deriving lock, action, tilemap, plane, pushwall activity, or held-button suppression metadata after the headless use dispatch has already identified the target.

Actor wake results similarly retain actor/player tile coordinates alongside before/after mode and direction, so a future presentation layer can explain chase activation without re-sampling mutable actor state after the wake transition.


## Live tick palette upload

The live tick output now feeds the existing palette-shifted upload metadata path. A headless test runs a food-pickup tick, consumes the returned white palette-shift result with `wl_describe_palette_shifted_texture_upload`, and verifies the selected palette/RGBA expansion hash. This keeps gameplay-driven palette selection connected to the future SDL3 texture upload boundary without needing SDL at test time.


## Live tick scene rendering

`wl_step_live_tick` state changes now feed directly into the live door-aware scene renderer. A headless test uses a keyed use-button tick to open a door, verifies the door progression clears the live collision tile in the same update, then renders the resulting open doorway scene through `wl_render_runtime_door_camera_scene_view`. This closes the loop from input-style tick state to renderer-facing wall+sprite output without requiring SDL3.


## Live tick pushwall scene rendering

Live tick state now drives moving-pushwall scene rendering too. A headless test runs a use-button tick that starts a pushwall and advances it across its first block boundary, then renders the resulting moving marker through `wl_render_runtime_door_camera_scene_view`. The tick-driven scene is asserted against the independently rendered moving-pushwall scene hash, keeping the input/state/render path deterministic and SDL-free.


## Live tick static pickup scene removal

Static pickup/removal now reaches the live renderer path. A headless test collects a bonus static sprite ref, decodes the matching local VSWAP sprite surface, renders the active static through `wl_render_runtime_door_camera_scene_view`, then runs `wl_step_live_tick` to pick it up and deactivate the runtime static. A second sprite-ref collection returns no visible static, and the same scene renders to a distinct empty-static hash.


## Actor bite damage seam

The gameplay layer now has a deterministic dog-bite/contact damage seam. `wl_try_actor_bite_player` mirrors the original `T_Bite` shape by checking player/actor proximity against the original tile-global/min-actor-distance range, applying a caller-supplied chance roll threshold, deriving bite damage from a caller-supplied damage roll, then routing the result through `wl_apply_player_damage` so difficulty scaling, death state, and red palette-shift counters stay on the same gameplay path.


## Actor shooting damage seam

The gameplay layer now has a deterministic `T_Shoot`-style actor shooting seam. `wl_try_actor_shoot_player` takes runtime actor/player descriptors plus explicit area/line-of-sight/running/visibility and random-roll inputs, computes the original distance-adjusted hit chance shape, applies SS/boss distance improvement, derives close/medium/far damage from the original roll shifts, and routes hits through `wl_apply_player_damage`.


## Projectile damage seam

The gameplay layer now has a deterministic projectile step seam. `wl_step_projectile` moves a projectile by caller-supplied fixed deltas with the original one-tile positive clamp shape, checks `PROJSIZE` wall collision against the live runtime tilemap, checks `PROJECTILESIZE` player impact range, maps needle/rocket/hrocket/spark/fire damage from the original random-roll formulas, and routes hits through `wl_apply_player_damage`.


## Live projectile tick seam

Projectile damage is now connected to a live tick orchestration path. `wl_step_live_projectile_tick` runs the same movement/use/door/pushwall work as `wl_step_live_tick`, steps an optional active projectile before palette advancement, and returns the final palette state so projectile impacts can select a red flash in the same headless frame.


## Live actor attack tick seam

Actor contact/ranged damage is now connected to live tick orchestration. `wl_step_live_actor_tick` runs the same movement/use/door/pushwall sequence as the base live tick, dispatches deterministic dog bites or shooter attacks with caller-supplied chance/damage rolls, then advances palette state so same-frame actor damage returns the red flash result future SDL3 presentation can upload.


## Live combat tick seam

Actor and projectile damage can now share one deterministic frame update. `wl_step_live_combat_tick` runs the live motion/use/door/pushwall sequence, optionally dispatches actor bite/shoot damage and an active projectile step, then advances palette state once after cumulative frame damage so future SDL3 presentation can consume one coherent red-flash result.


## Live combat upload seam

The combined combat tick now feeds the SDL-free palette upload boundary. A headless test routes cumulative actor/projectile damage through `wl_step_live_combat_tick`, then passes the returned red palette state to `wl_describe_palette_shifted_texture_upload` and RGBA expansion so future SDL3 presentation can consume the same deterministic upload metadata.


## Actor damage state seam

Actor hitpoints and kill effects now have a deterministic pure-C state seam. `wl_init_actor_combat_state` applies original difficulty-based starting hitpoints, and `wl_apply_actor_damage` models doubled pre-attack damage, attack-mode start on nonlethal hits, kill score awards, shootable/alive clearing, and representative drop selection without coupling to original state tables or sound/status-bar code.


## Actor drop static seam

Actor kill drops now enter the mutable runtime model as pickup statics. `wl_spawn_actor_drop_static` converts `wl_apply_actor_damage` drop metadata into active nonblocking bonus statics at the actor tile, so killed guards/SS/bosses can produce the same pickup path later consumed by movement, scene refs, and SDL-free render/upload tests.


## Live actor damage/drop tick seam

Actor damage and kill drops now have a live frame-transition seam. `wl_step_live_actor_damage_tick` runs the same headless movement/use/door/pushwall sequence as the other live tick helpers, applies `wl_apply_actor_damage`, converts killed-actor drop metadata into runtime statics with `wl_spawn_actor_drop_static`, and advances palette state once. This keeps score awards, actor alive/shootable clearing, drop spawning, and future renderer input connected before SDL3 presentation.


## Live actor drop scene seam

Live-spawned actor drops now reach the renderer-facing path. A headless test kills a guard through `wl_step_live_actor_damage_tick`, collects the spawned clip drop through `wl_collect_scene_sprite_refs`, decodes the referenced local VSWAP sprite surface, and renders it with `wl_render_runtime_door_camera_scene_view`. The committed proof is metadata plus scene hash `0x707dbe4e`, not decoded asset bytes.


## Full live combat tick seam

`wl_step_live_full_combat_tick` combines the current headless frame seams into one broader gameplay transition: movement/use/doors/pushwalls, optional outgoing actor damage and drop spawning, optional actor bite/shoot damage, optional projectile damage, and one palette update after all frame damage. The test covers same-frame guard kill/drop, dog bite, needle projectile hit, red flash selection, score/health mutations, projectile removal, and a null-combat no-op path.


## Actor death-state seam

Actor kill effects now have deterministic death animation metadata. `wl_start_actor_death_state` and `wl_step_actor_death_state` model the original death `statetype` chains for guards, dogs, SS, mutants, officers, and the representative WL6 boss path: initial death-scream flag, sprite source indices, per-stage tic durations, and final dead-frame behavior. This keeps death visuals testable before wiring them into live scene refs or SDL3 presentation.


## Actor death scene seam

Death-state actor sprites now reach the renderer-facing scene path. `wl_build_actor_death_scene_ref` converts killed-actor death-state metadata into a scene ref, and the headless test decodes the referenced local VSWAP sprite surface before rendering it with `wl_render_runtime_door_camera_scene_view`. The committed evidence is metadata plus scene hash `0x2e8b4819`, keeping proprietary sprite bytes local.


## Full combat death-ref seam

The full live combat tick now returns death-state presentation metadata in the same frame as actor kill/drop spawning. When `wl_step_live_full_combat_tick` kills a target actor, its result includes the started `wl_actor_death_state` plus a renderer-facing `wl_scene_sprite_ref` for the current death frame, alongside drop, score, incoming-damage, projectile, and palette output.


## Full combat death-ref scene seam

Full live combat death refs now drive the renderer-facing path directly. A headless test kills a guard via `wl_step_live_full_combat_tick`, decodes the returned death-frame VSWAP chunk locally, and renders the resulting surface through `wl_render_runtime_door_camera_scene_view`. The scene hash matches the independent death-state render hash `0x2e8b4819`.


## Actor death final-frame actor refs

Runtime actor scene refs now support a final death-frame replacement. `wl_apply_actor_death_final_frame` consumes a finished `wl_actor_death_state`, makes the target actor slot inert/non-shootable, and enables an actor scene-source override so `wl_collect_scene_sprite_refs` emits the final death sprite for the same model index. The first headless check verifies a guard ref changes from source `50` to final source `95` / VSWAP chunk `201`.


## Live actor death tick

Actor death animation now has a live tick seam. `wl_step_live_actor_death_tick` advances a `wl_actor_death_state`, returns a renderer-facing scene ref for the current death frame, and applies the persistent final-frame override to the actor slot when the sequence finishes. The guard test advances source `91` → `92` → final `95` / VSWAP chunk `201`.


## Full combat plus death tick

Full live combat orchestration can now also advance an active death animation in the same frame. `wl_step_live_full_combat_death_tick` wraps the existing full-combat frame transition, then optionally calls the live actor death tick to return current death refs and apply final corpse-frame overrides. The guard coverage preserves a spawned drop while finalizing model index `9` to source `95` / VSWAP chunk `201`.


## Full combat death-tick final scene

The combined full-combat/death-tick output now drives headless scene rendering. The test kills a guard, advances the active death state through `wl_step_live_full_combat_death_tick`, decodes the returned final corpse-frame VSWAP chunk, and renders it through `wl_render_runtime_door_camera_scene_view` with stable scene hash `0x81c10dcf`.


## Broadened actor death final refs

Final corpse-frame actor refs now have coverage beyond the guard path. Headless tests finalize officer, SS, dog, mutant, and boss death states and verify the scene-ref source/VSWAP metadata emitted by `wl_collect_scene_sprite_refs`: officer `284/390`, SS `183/289`, dog `134/240`, mutant `233/339`, and boss `303/409`.


## Patrol path direction seam

`wl_summarize_runtime_tile_quadrants` now adds a compact live-tilemap locality check: solid-wall and open-floor counts are split across the four 32x32 map quadrants and cross-checked against the full runtime tile summary. This gives future spawn, collision, and renderer work a low-cost way to notice quadrant-level map-state drift.

`wl_summarize_runtime_tile_center` adds a focused 16x16 center-window diagnostic for the mutable runtime tilemap, splitting clear floor, solid walls, door markers, pushwall markers, and other markers. This gives player-spawn, collision, and presentation work a cheap headless guard for central map-state mutations without scanning the whole map at the call site.

`wl_summarize_runtime_player_cardinal_tiles` complements the 3x3 player-neighborhood diagnostic with just the north/east/south/west runtime tiles. It clips at map edges, reports which cardinal samples exist, and buckets clear floor, solid walls, door markers, pushwall markers, and other markers for movement/input checks that only care about direct step targets.

`wl_select_path_direction` now models the first `SelectPathDir` decision boundary for patrol actors in a headless-friendly way: path markers at the actor tile can replace the current direction, the next tile is checked against the mutable runtime tilemap, blocked/out-of-bounds moves return `WL_DIR_NONE`, and unsupported diagonal marker arrows are preserved in metadata but do not yet produce movement.


## Patrol actor step seam

`wl_step_patrol_actor` now applies the path-direction selection seam to a runtime actor slot: patrol actors update direction and move one tile when the selected path is clear, report blocked state without mutation when the next tile is solid/out of bounds, and reject non-patrol or invalid actor slots. This remains a discrete headless bridge before full original `T_Path` tic-distance movement.


## Patrol actor scene refs

Patrol stepping now feeds renderer-facing actor refs. Headless tests step a patrol actor through marker-selected movement, collect `wl_scene_sprite_ref` output after movement and after a blocked step, and verify the patrol guard source/chunk plus world-center coordinates update or persist deterministically.


## Patrol actor scene render

Patrol actor scene-ref metadata now reaches the headless wall+sprite scene renderer. The test projects a patrol guard ref through `wl_render_runtime_door_camera_scene_view`, verifies source/visibility metadata, and keeps the canvas hash stable against the established open-door scene path.


## Patrol actor tic stepping

`wl_step_patrol_actor_tics` now wraps the one-tile patrol step with a `speed * tics` movement budget. Whole-tile movement advances through repeated patrol steps, partial movement is preserved as leftover metadata, and blocked paths stop without moving the actor. This is still tile-granular, but follows the original `T_Path` loop shape before adding fine-position distance accumulation.


## Patrol actors batch tick

`wl_step_patrol_actors_tics` now applies the tic-budgeted patrol seam across all `WL_ACTOR_PATROL` runtime actors while skipping stand/inert actors. The aggregate result reports considered, stepped, blocked, and total tile-step counts for a future broader live actor AI tick.


## Live actor AI patrol tick

`wl_step_live_actor_ai_tick` now folds patrol actor ticking into the same headless live-frame boundary used for player motion/use/doors/pushwalls/palette. It returns normal live tick output plus aggregate patrol AI movement metadata, giving future SDL3 presentation one frame result for environmental and patrol actor updates.


## Live AI patrol scene refs

The live actor AI patrol wrapper now has renderer-facing ref coverage. After `wl_step_live_actor_ai_tick` mutates patrol actor tiles, `wl_collect_scene_sprite_refs` emits stable guard source/VSWAP metadata and updated world coordinates, including blocked-step preservation.


## Live AI patrol rendering

Live-AI-updated patrol refs now feed the door-aware wall+sprite scene renderer. The headless test decodes the moved guard sprite from local VSWAP data, renders it through `wl_render_runtime_door_camera_scene_view`, and asserts stable projection metadata plus canvas hash `0x6ee1f8bf`.


## Patrol fine-position refs

Patrol actors now carry optional fixed-point `fine_x`/`fine_y` coordinates. Tic-budgeted patrol movement reports and stores partial-tile positions, while scene-ref collection prefers fine coordinates when present so render-facing actor placement can advance before a whole tile is consumed.


## Live AI fine-position patrol rendering

Live actor AI now drives sub-tile patrol placement into rendering. A half-tic patrol budget leaves the actor on its current tile, emits fine world coordinates through scene refs, decodes the guard sprite locally, and renders a stable door-aware scene hash `0xcf61b07b`.


## Patrol remainder accumulation

Patrol actors now retain sub-tile movement remainder across tic calls. Two half-tile budgets accumulate into a whole-tile step, then reset the renderer-facing fine position to the new tile center while keeping blocked movement deterministic.


## Live AI remainder rendering

Accumulated patrol remainder now survives through two live AI frames into rendering: the first half-tile frame renders fine coordinates with hash `0xcf61b07b`, the second half-tile frame consumes a full tile and renders the tile-centered guard with hash `0x6ee1f8bf`.


## Live AI dog fine-position rendering

Live AI fine-position rendering now covers a second patrol actor class. A patrol dog uses source/chunk `99/205`, sub-tile world coordinates, local VSWAP decode, and a stable door-aware scene hash `0x08ab64f0`.


## Live AI officer fine-position rendering

Live AI fine-position rendering now covers patrol officers. Officer refs use source/chunk `246/352`, sub-tile coordinates from the half-tile patrol budget, local VSWAP decode, and stable scene hash `0xa6544334`.


## Live AI SS fine-position rendering

Live AI fine-position rendering now covers patrol SS actors. SS refs use source/chunk `146/252`, sub-tile coordinates from the half-tile patrol budget, local VSWAP decode, and stable scene hash `0x0b6fe30b`.


## Live AI mutant fine-position rendering

Live AI fine-position rendering now covers patrol mutants. Mutant refs use source/chunk `195/301`, sub-tile coordinates from the half-tile patrol budget, local VSWAP decode, and stable scene hash `0x96655cea`.


## Live AI boss fine-position rendering

Live AI fine-position rendering now covers a boss patrol-style actor. Boss refs use Hans source/chunk `296/402`, sub-tile coordinates from the half-tile patrol budget, local VSWAP decode, and stable scene hash `0x731d6cb3`.


## Chase direction selection seam

A deterministic `SelectChaseDir`-style helper now chooses direct, non-turnaround chase directions toward the player, falls back through old/random-search directions, and reports blocked results without touching SDL or original source.


## Chase actor step seam

Chase direction selection now mutates a runtime actor through `wl_step_chase_actor`, updates tile and fine coordinates, rejects non-chase actors, and feeds renderer-facing scene refs with chase walk sprite metadata.


## Chase tic fine-position seam

Chase movement now has a tic-budgeted fine-position seam. `wl_step_chase_actor_tics` preserves partial movement, continues the same direction across remainder completion, consumes whole tiles, and exposes renderer-facing fine coordinates.


## Chase fine-position rendering

Chase fine-position actor refs now feed the headless scene renderer. A half-tile chase guard keeps source/chunk `58/164`, emits fine world coordinates `0x58000/0x50000`, and renders with stable scene hash `0xa71311c2`.


## Live AI chase fine-position rendering

The live actor AI wrapper now advances chase actors as well as patrol actors. A half-tile chase guard passes through `wl_step_live_actor_ai_tick`, emits source/chunk `58/164` at `0x58000/0x50000`, and renders with stable hash `0xa71311c2`.


## Live AI chase remainder rendering

Live AI chase rendering now covers two half-tile frames. The first frame stores chase remainder and renders fine coords with hash `0xa71311c2`; the second consumes the carried movement into tile-centered coords `0x58000/0x48000` with hash `0x4a4c3e4f`.


## Live AI chase combat bridge

Live AI chase movement now feeds combat attack selection in the same deterministic harness. A chase guard completes its accumulated tile step, then the moved actor is used by `wl_step_live_actor_tick` for a distance-2 shot that applies a red palette shift.


## Live AI dog chase bite bridge

Chase-combat coverage now includes a dog actor. Two half-tile live AI chase frames complete a dog step, then the moved dog feeds the actor tick bite path with deterministic damage and red palette state.


## Live AI shooter-class chase combat

Chase-combat coverage now includes officer, SS, mutant, and boss shooter classes. Each case completes two half-tile live AI chase frames, then verifies the moved actor can shoot with deterministic original-style distance scaling and damage.


## Live AI chase full-combat bridge

A chase-moved guard now feeds `wl_step_live_full_combat_tick` in one deterministic frame: the moved actor shoots the player while the same actor damage/death/drop path starts its death state and renderer-facing death ref.


## Live AI chase full-combat render

The chase full-combat seam now reaches the headless scene renderer: a chase-moved guard can shoot, die, spawn a drop, and render the death/drop refs together with stable scene hash `0x4a76f09a`.


## Live AI chase death-final render

Chase-started full combat now advances through the combined death-tick final-frame path. The moved guard death ref switches from `91/197` to final corpse `95/201`, keeps the spawned drop visible, and renders with stable hash `0x8a2741bf`.


## Live AI chase death-transition render

The chase-started death render now explicitly verifies the rendered transition from initial death+drop hash `0x4a76f09a` to final corpse+drop hash `0x8a2741bf`, and checks it differs from the older no-drop final-corpse scene.


## Shooter-class chase death-final metadata

Chase-started full-combat/death-final coverage now extends from guards to officer, SS, mutant, and boss shooter classes. Each class completes chase movement, takes lethal damage, advances death ticking, and verifies the expected final corpse source/chunk override.


## Shooter-class chase death-final render hashes

Chase-started death-final rendering now includes officer, SS, mutant, and boss final corpse/drop scenes with stable hashes: officer `0x9b24b352`, SS `0x5b093720`, mutant `0xbfccde1b`, boss `0xc6d3eb4d`.


## Dog chase death-final render

Chase-started death-final rendering now covers dogs as a no-drop contact enemy. The dog completes live AI chase movement, dies through full-combat/death ticking, and renders final corpse `134/240` with stable hash `0x92ff40dd`.


## Headless presentation frame descriptor

Added an SDL-free presentation descriptor that wraps a rendered indexed frame with upload metadata, viewport size, pixel hash, palette hash, and palette-shift metadata. This gives a small deterministic boundary for a future SDL3 texture upload without requiring SDL3 dev files.


## Palette-shifted present frame descriptor

The presentation descriptor now verifies palette-shifted output as well as base palettes. A red-shifted dog chase death frame preserves pixel hash `0x92ff40dd` while selecting red palette hash `0x90a6cdc5` and recording shift kind/index.


## Combat palette present frame descriptors

Present-frame descriptor coverage now includes both red and white combat palette shifts. The same indexed dog chase death frame keeps pixel hash `0x92ff40dd` while selecting red hash `0x90a6cdc5` or white hash `0x3c8da1ed` with explicit shift kind/index metadata.


## Live combat present-frame descriptor

Live combat palette output now feeds `wl_describe_present_frame` directly. A deterministic dog/projectile combat tick produces a red-shifted descriptor with viewport `4x4`, indexed pixel hash from the sample frame, palette hash `0x35132dc5`, and shift index `3`.

## Optional SDL3 bootstrap and smoke test

The default `make test` gate remains SDL-free. On machines without system SDL3 development files, use the repo-local bootstrap script to build SDL3 into ignored `.deps/` paths:

```bash
scripts/bootstrap_sdl3.sh
cd source/modern-c-sdl3
make test-sdl3
```

`make test-sdl3` auto-detects `sdl3.pc` from `../../.deps/SDL3` or `PKG_CONFIG_PATH`. If SDL3 is unavailable, it prints a skip message and exits successfully. When available, it runs a hidden-window smoke test with `SDL_VIDEODRIVER=dummy` so the first presentation boundary remains headless-friendly.


## Chase attack scene present descriptor

A rendered live-AI chase attack scene now feeds `wl_describe_present_frame` with the actual red palette shift emitted by actor shooting. The descriptor pins viewport `80x128`, scene pixel hash `0x4a4c3e4f`, red shift metadata, and selected upload palette pointer/hash.

`make test-sdl3` currently runs two optional SDL3 checks when SDL3 is available:

- `test_sdl3_smoke` initializes SDL video and updates a hidden dummy-driver window.
- `test_sdl3_present` expands a deterministic indexed frame to RGBA, blits it to a hidden SDL3 window surface, and updates it.

`test_sdl3_present` now uses actual local WL6 `VSWAP.WL6` data: it decodes wall page 0, routes it through the existing present-frame descriptor and RGBA expansion seam, then blits it to a hidden SDL3 window surface under the dummy video driver. The test commits only hashes/metadata, not decoded game pixels.


## SDL3 screenshot artifact smoke

`make test-sdl3-present` now writes `build/wolf-wall-present.bmp` from the actual WL6 wall-frame present path under the SDL dummy video driver. The file is generated inside ignored `build/` for local inspection only; committed checks remain metadata/hash assertions.


## SDL3 palette screenshot smoke

`make test-sdl3-present` now emits both base and red-shifted ignored BMP artifacts from the WL6 wall present path: `build/wolf-wall-present.bmp` and `build/wolf-wall-present-red.bmp`. The red path verifies palette descriptor metadata and RGBA hash before saving.


## SDL3 screenshot artifact hashes

`make test-sdl3-present` now verifies the generated BMP artifact bytes: base `build/wolf-wall-present.bmp` size/hash `16522` / `0xb49b4cbf`, red `build/wolf-wall-present-red.bmp` size/hash `16522` / `0xaa1c75c5`.


## SDL3 wall atlas screenshot smoke

`make test-sdl3-present` now also generates `build/wolf-wall-atlas-present.bmp` from a two-wall WL6 atlas. The atlas remains generated-only and ignored; tests pin indexed, RGBA, and BMP artifact hashes.


## SDL3 sprite screenshot smoke

`make test-sdl3-present` now emits `build/wolf-sprite-present.bmp` from decoded VSWAP sprite chunk `106` rendered onto a `128x64` indexed canvas. The screenshot and decoded pixels stay ignored; tests pin sprite, canvas, RGBA, and BMP artifact hashes.

## Live player fire tick bridge

`wl_step_live_player_fire_tick` now folds optional player weapon fire into the existing headless live-frame boundary. Tests cover fire-button machinegun ammo consumption, active-attack refire suppression via `fire_blocked_by_active_attack` including the tick where the prior attack finishes, no-ammo fallback to knife, and no-fire frames while preserving normal live tick/palette metadata.

## Actor mode count helper

`wl_count_actors_by_mode` mirrors the actor-kind summary helper for runtime actor modes. The broader WL6 map coverage now verifies mode histograms against the built model data and rejects undersized output capacity, giving future AI/live-tick work a stable headless summary seam.

## Actor flag summary helper

`wl_summarize_actor_flags` reports deterministic aggregate actor flags (shootable, ambush, kill-total, scene override, and inert counts) for a built runtime model. The broader WL6 map coverage now cross-checks those summary counts against actor descriptors and model kill totals, giving live AI/death/render seams a compact headless sanity check before mutating actor state.

`wl_summarize_path_marker_source_tiles` reports AI patrol marker provenance for original arrow tiles `90..97`, including unique, zero, unexpected, and min/max source-tile counts. It mirrors the pushwall source-tile diagnostic so patrol/chase work can sanity-check path metadata without mutating runtime state.

## Actor position summary helper

`wl_summarize_actor_positions` reports aggregate actor position state: actors moved from spawn, actors with fine/patrol-position state, and spawn/current tile bounds failures. The broader WL6 map coverage cross-checks the summary against model descriptors and pins initial patrol movement counts for future AI interpolation work.

`wl_summarize_actor_wake_state` reports whether shootable live actors are immediately wakeable, waiting behind ambush gating, already chasing, or ineligible for wake-up. The same broader WL6 map sweep cross-checks both ambush-excluded and ambush-included views against actor descriptors before any mutating wake pass runs.

`wl_summarize_actor_patrol_paths` reports aggregate patrol path readiness before mutating actor movement: patrol actors with a selected path direction, blocked/no-direction patrol actors, and invalid-position patrol actors. The broader WL6 map sweep verifies the summary against `wl_select_path_direction()` for every patrol actor.

## Actor scene-source summary helper

`wl_summarize_actor_scene_sources` reports runtime actor scene-source readiness: total actors, default sprite-source mappings, explicit override mappings, and actors missing source metadata. The broader WL6 map sweep cross-checks those buckets against model descriptors, giving renderer-facing actor-state work a small deterministic guardrail.

- `wl_summarize_actor_engagements` now reports both nearest and farthest active threat descriptors for headless actor-state diagnostics.

- Added a headless actor motion summary seam that partitions centered actors, sub-tile offset actors, active movement remainders, invalid actor positions, farthest sub-tile offset actor/index, and active remainder min/max ranges for runtime state progression diagnostics.
- Runtime model diagnostics include actor activity summaries for active/waiting/inert/combat-ready/boss-or-ghost buckets.
- `wl_summarize_actor_modes` partitions live actor descriptors by exact runtime mode and separately reports invalid modes/positions for state-progression diagnostics.
- Added `wl_summarize_actor_tile_occupancy` to report occupied actor tiles, stacked actors, invalid positions, and max stack depth for headless collision/progression diagnostics.
- Runtime model diagnostics include actor spawn occupancy summaries for original spawn-tile stacking, invalid spawn coordinates, moved-from-spawn counts, and max spawn stack depth.
- Runtime model diagnostics include actor collision-tile summaries for open, wall, door, door-adjacent, and invalid actor positions before live collision/AI mutation.
- Runtime model diagnostics include actor/player adjacency summaries for same-tile, cardinal-adjacent, diagonal-adjacent, same-row/column, distant, and invalid actor positions before live bite/chase state mutation.
- Runtime model diagnostics include actor/player line-of-sight summaries for clear cardinal, wall-blocked, door-blocked, same-tile, non-cardinal, and invalid actor positions before live chase/shoot mutation.

`wl_summarize_actor_source_tiles` reports actor source-tile coverage (total actors, unique source tiles, zero-source placeholders, and min/max source tile) as a compact non-mutating diagnostic for checking model construction against map semantics before renderer or AI code consumes actor descriptors. Synthetic headless coverage pins duplicate/zero/min/max behavior and null-argument rejection.
- Runtime model diagnostics include actor door-proximity summaries for actors on door tiles, door-adjacent marker tiles, away from doors, invalid positions, and unique touched door indices before live door/AI collision mutation.
- Runtime model diagnostics include actor combat-class summaries for infantry, dog, boss, ghost, corpse, shootable, kill-credit, noncombat, and invalid-kind buckets before live combat/death/drop mutation.

- `wl_summarize_actor_threats` partitions shootable actors into immediate, latent, ambush-latent, and inert threat buckets while counting non-shootable actors and flagging unknown shootable actor modes, giving live AI/combat work a small SDL-free state sanity check.
- `wl_summarize_actor_attack_readiness` combines shootable active actor state, facing direction, and cardinal line-of-sight into ready/not-ready buckets so future attack ticks can be checked headlessly before mutating combat state.

## Runtime static state summary

`wl_summarize_static_states` now provides an SDL-free static-object diagnostic for live pickup/render readiness, partitioning active/inactive, blocking, bonus, treasure, active-bonus, and active-blocking static descriptors with synthetic headless coverage.

## Runtime static source-tile summary

`wl_summarize_static_source_tiles` mirrors the actor source-tile diagnostic for runtime statics, reporting total statics, unique/zero source tiles, and min/max source tile values so SOD/WL6 static classification drift can be caught without mutating the model.

`wl_summarize_static_tile_occupancy` adds a matching static placement diagnostic, reporting occupied map tiles, stacked static descriptors, invalid coordinates, and maximum stack depth without mutating pickup/render state.

## Runtime static/player distance summary

`wl_summarize_static_player_distances` now gives pickup/render code an SDL-free way to inspect nearest and farthest static objects relative to the player, with active-only filtering and invalid-coordinate accounting covered by synthetic headless tests.

## Runtime door state summary

`wl_summarize_door_states` provides an SDL-free live-door diagnostic for presentation and collision work, partitioning vertical/horizontal doors, locked/unlocked doors, open/closed/opening/closing actions, moving and partially-open doors, invalid action values, and maximum door position with synthetic headless coverage.

`wl_summarize_door_timing` complements that state snapshot with non-mutating ticcount buckets for waiting, countdown, overdue, moving-with-countdown, and open-with-countdown doors, plus min/max ticcount bounds. This gives future door orchestration and presentation seams a deterministic way to sanity-check live timing metadata without advancing the door state.

## Runtime door/player distance summary

`wl_summarize_door_player_distances` reports nearest/farthest door tiles from a player tile, orientation buckets, and invalid door positions without mutating runtime state. Synthetic headless coverage pins empty/invalid behavior for future door-aware AI, collision, and presentation diagnostics.

## Runtime pushwall state summary

`wl_summarize_pushwall_state` reports pushwall marker direction buckets, invalid marker coordinates, and active moving-pushwall metadata/validity without advancing the runtime pushwall. Synthetic headless coverage pins marker direction counts plus active motion marker/position validation for future pushwall AI/collision/presentation checks.

## Runtime pushwall/player distance summary

`wl_summarize_pushwall_player_distances` reports nearest/farthest pushwall markers relative to a player tile while accounting for invalid marker coordinates. The helper mirrors the door/static distance diagnostics and gives pushwall collision/presentation work a deterministic SDL-free proximity seam.

`wl_summarize_door_locks` provides an SDL-free diagnostic for door lock metadata before presentation/input glue consumes it. It reports normal, keyed, and elevator/other lock buckets plus unique/min/max lock ids, and detects mismatches between runtime descriptor locks and original door source tiles with synthetic headless coverage.

## Door source tile diagnostic

`wl_summarize_door_source_tiles` adds a small read-only inspection seam for runtime door descriptors. It verifies the original source-tile range used for normal/keyed/elevator doors, partitions vertical versus horizontal source ids, records duplicate/unique source coverage, and reports min/max source bounds. The synthetic headless test covers valid, invalid, duplicate, and empty-door cases without touching `source/original/` or requiring SDL3.

## Runtime pushwall source-tile summary

`wl_summarize_pushwall_source_tiles` reports pushwall marker source-tile provenance without mutating runtime state. It counts the original info-plane marker tile `98`, duplicate/unique source values, and zero/unexpected source tiles so future pushwall setup and diagnostics can distinguish valid secrets from malformed markers headlessly.

## Runtime pushwall tile occupancy summary

`wl_summarize_pushwall_tile_occupancy` reports unique, duplicate, and invalid pushwall marker positions without mutating runtime state. This mirrors the path-marker occupancy diagnostic and gives secret-wall setup/collision work a deterministic way to catch overlapping or out-of-bounds pushwall markers headlessly.

## Runtime static/player adjacency summary

`wl_summarize_static_player_adjacency` mirrors the actor/player adjacency diagnostic for static descriptors, giving pickup/render code an SDL-free same-tile/adjacent/row-column/distant proximity summary with active-only filtering and invalid-coordinate accounting.

## Runtime pushwall/player adjacency summary

`wl_summarize_pushwall_player_adjacency` reports same-tile, cardinal-adjacent, diagonal-adjacent, same-row/column, distant, and invalid pushwall marker buckets relative to the player without mutating pushwall state. Synthetic headless coverage pins every bucket for future pushwall collision/presentation work.

## Runtime path-marker/player adjacency summary

`wl_summarize_path_marker_player_adjacency` reports same-tile, cardinal-adjacent, diagonal-adjacent, same-row/column, distant, and invalid patrol marker buckets relative to the player without mutating AI path state. Synthetic headless coverage pins every bucket for future patrol wake/chase diagnostics.

`wl_summarize_path_marker_line_of_sight` mirrors the actor/static/door/pushwall cardinal visibility buckets for patrol markers, including wall-blocked, door-blocked, same-tile, non-cardinal, and invalid-marker cases. This gives patrol-route diagnostics a deterministic SDL-free player visibility seam.

## Runtime path-marker direction summary

`wl_summarize_path_marker_directions` reports north/east/south/west/no-direction/invalid patrol-marker direction buckets plus invalid marker coordinates without mutating AI path state. Synthetic headless coverage pins every bucket for future patrol/chase progression diagnostics.

`wl_summarize_path_marker_exits` adds a non-mutating patrol-marker diagnostic that checks each marker's directed next tile against the live tilemap and reports open, blocked, out-of-bounds, no-direction, invalid-direction, and invalid-marker counts for headless AI path validation.

## Runtime path-marker/player distance summary

`wl_summarize_path_marker_player_distances` reports nearest/farthest patrol markers relative to a player tile while accounting for invalid marker coordinates. Synthetic headless coverage pins valid, invalid-argument, and no-valid-marker behavior for future patrol/chase diagnostics.

`wl_summarize_path_marker_tile_occupancy` provides a small SDL-free patrol-marker diagnostic for total markers, valid unique marker tiles, duplicate placements, and invalid coordinates. This keeps path-arrow setup checks deterministic before deeper patrol/chase state progression.

## Runtime path-marker chain summary

`wl_summarize_path_marker_chains` follows each patrol marker's outgoing arrow and summarizes linked next-marker continuity as straight, left, right, reverse, or dangling, with no-direction/invalid buckets. Synthetic headless coverage pins every bucket for future patrol path progression work.

## Runtime model capacity summary

`wl_summarize_model_capacity` provides a read-only capacity snapshot for the fixed-size runtime descriptor arrays: doors, statics, actors, path markers, and pushwalls. It reports current counts, remaining slots, full flags, and unknown-info-tile count so future broader-map and boss/Spear classification work can spot descriptor pressure headlessly before adding more mutable runtime behavior.

The WL6 runtime-model sweep now covers all four difficulty levels across all 60 gameplay maps for unknown info-plane classifications, not just easy difficulty. This keeps boss/Spear-style tile classification regressions visible before future actor-state or presentation work consumes the model.

`wl_summarize_door_area_connections` adds a compact read-only diagnostic for the SetupGameLevel-style door area graph. It reports valid, unique, duplicate, self, and invalid area links plus the area id span, giving future door/visibility work a small headless sanity check before mutating area connectivity at runtime.

`wl_summarize_actor_door_blockers` reports actors currently occupying runtime door-center tiles, splitting shootable vs non-shootable blockers and unique/min/max blocked door indices before future close-door or AI collision mutation.

`wl_summarize_door_area_matrix` complements the door-list diagnostic by checking the cached area adjacency matrix directly. It reports directed/undirected links, asymmetric entries, self-links, area span, and max link weight so future door visibility or sound-propagation code can validate the matrix produced by model setup before mutating it.

`wl_summarize_pushwall_motion_path` inspects the active moving-pushwall record without mutating it, projecting the next tile from direction and reporting in-bounds/open/blocked status plus tile value and sub-tile position. Synthetic headless coverage pins blocked, open, edge, inactive, and invalid-argument behavior for future pushwall collision/presentation work.

## Runtime unknown info-tile summary

`wl_summarize_unknown_info_tiles()` exposes the runtime model's recorded unknown info-plane classification metadata as a small SDL-free diagnostic: count, hash, first/last tile coordinates, and a presence flag. This keeps future boss/SOD classification sweeps testable without depending on autopilot state files.

## Runtime door tile occupancy summary

`wl_summarize_door_tile_occupancy` reports total, unique, duplicate, and invalid door descriptor positions without mutating runtime state. This mirrors the static/path-marker/pushwall occupancy diagnostics and gives future door collision or close-door logic a deterministic headless check for overlapping or out-of-bounds door centers.

## Runtime static line-of-sight summary

`wl_summarize_static_line_of_sight` mirrors actor cardinal visibility diagnostics for static descriptors, including active-only filtering and wall/door blockers. This gives pickup/render preparation a deterministic SDL-free summary of statics that are in the player's row or column before presentation work mutates or draws them.

## Runtime door line-of-sight summary

`wl_summarize_door_line_of_sight` applies the same non-mutating cardinal blocker diagnostic to runtime door descriptors. It reports clear, wall-blocked, door-blocked, same-tile, non-cardinal, and invalid door positions so door-aware AI/presentation work can sanity-check direct player-to-door visibility without advancing live door state or requiring SDL3.

`wl_summarize_pushwall_line_of_sight` reports clear, wall-blocked, door-blocked, same-tile, non-cardinal, and invalid pushwall marker sight-line buckets relative to the player without mutating runtime pushwall state. Synthetic headless coverage mirrors the door line-of-sight diagnostic for future pushwall interaction/render checks.

- `wl_summarize_actor_spawn_deltas` reports whether live actors remain on their spawn tile, moved along a cardinal line, moved diagonally, or carry invalid spawn/current coordinates, plus the farthest moved actor by Manhattan delta. This gives chase/patrol progression tests a compact non-mutating sanity check for spawn-to-runtime drift.

## Runtime actor/path-marker summary

`wl_summarize_actor_path_markers` reports patrol actors that are directly on a path marker, cardinal-adjacent or diagonal-adjacent to one, missing nearby marker guidance, or carrying invalid current coordinates. This gives patrol/chase progression work a compact non-mutating check that live actor positions still line up with the map-derived patrol route graph.

- `wl_summarize_runtime_tiles` reports clear floor, solid wall, door-marker, moving-pushwall-marker, and other runtime marker buckets from the mutable tilemap, plus first/last solid wall tile ids. Synthetic and real WL6 headless coverage pin the summary as a compact sanity check for future live door/pushwall/render orchestration.

## Runtime tile edge summary

`wl_summarize_runtime_tile_edges()` complements the live tilemap bucket summary with a boundary-focused diagnostic. It counts boundary solid walls, clear floor, door markers, moving pushwall markers, other markers, and interior solid walls so future collision/raycast assumptions can quickly verify that loaded maps preserve the expected closed outer wall while still pinning marker classification behavior headlessly.

## Present upload row hashing

`wl_hash_texture_upload_rows` now hashes only meaningful upload row bytes for both indexed and RGBA descriptors, ignoring pitch padding while validating palette metadata for indexed uploads. Headless coverage pins the present-frame indexed hash and a padded RGBA upload hash so future SDL3 texture/surface handoffs can compare frame contents independently of backend pitch alignment.
`wl_summarize_runtime_tile_diagonals` adds a small live-tilemap diagonal diagnostic, partitioning the two corner-to-corner diagonals into solid walls, clear floor, and runtime markers. It also splits marker totals into door, moving-pushwall, and other marker buckets so spawn/camera/raycast smoke work can cheaply distinguish blocker classes without rescanning the tilemap.

`wl_summarize_runtime_tile_axes` complements the diagonal check with center row/column buckets for solid walls, clear floor, and markers. The headless coverage verifies real-map totals plus synthetic wall/marker/open cases so future spawn, raycast, and minimap work can cheaply detect crosshair-axis tilemap drift.

`wl_summarize_runtime_tile_corners` adds a four-corner live tilemap diagnostic that records each corner tile and buckets the corners as walls, clear floor, door markers, pushwall markers, or other markers. This gives boundary/collision smoke checks a tiny SDL-free guard for corner corruption without rescanning the full edge.

`wl_summarize_runtime_player_tile_neighborhood` adds a compact player-centered 3x3 live-tilemap diagnostic. It clips at map edges, reports the player's current tile, and splits nearby tiles into clear floor, solid walls, door markers, pushwall markers, and other markers so movement/collision/presentation work can sanity-check immediate player surroundings headlessly.
`wl_summarize_runtime_player_facing_tile` reports the single live tile in the player's current facing direction, including target coordinates and wall/door/pushwall/other/open category flags, giving use-button and collision preflight code a deterministic SDL-free probe.

## Runtime static collision summary

`wl_summarize_static_collisions` partitions static descriptors into active/inactive blocking, bonus, and dressing buckets with invalid-coordinate accounting. This keeps player collision and visible pickup preparation inspectable in headless tests without mutating runtime state.

- Added a headless player-facing run diagnostic that walks the live tilemap from the player until the first blocker or map edge, giving use/raycast preflight code a deterministic corridor summary without mutating runtime state.
- Added `wl_summarize_runtime_player_facing_actors`, a non-mutating player-facing actor diagnostic that counts unblocked actors on the player's current cardinal ray, shootable actors, map-blocked actors, invalid actor positions, and the nearest visible actor. This gives future use/combat/render preflight code a deterministic SDL-free line-of-fire summary.
- Added `wl_summarize_runtime_player_facing_statics`, a non-mutating player-facing static diagnostic that counts unblocked active statics on the player's current cardinal ray, blocking/bonus statics, inactive statics, map-blocked statics, invalid static positions, and the nearest visible static. This complements the facing actor probe for deterministic pickup/render/use preflight.
- Added `wl_summarize_runtime_player_interaction`, a combined player-facing preflight that reports the nearest visible actor/static candidate plus first map blocker metadata. The summary now also carries ahead/blocked actor/static counts, invalid/inactive descriptor counts, nearest source/tile/world coordinates, first map-blocker distance, and blocker class flags for solid walls, door markers, moving-pushwall markers, and other runtime markers so future use/combat/HUD prompts can distinguish an empty sightline from map-blocked or malformed candidates with one SDL-free query and no live-state mutation.
