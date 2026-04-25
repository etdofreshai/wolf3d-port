# Modern C + SDL3 Port

Target implementation for the Wolfenstein 3D modern C port using SDL3.

Goals:

- Stay close to original Wolfenstein 3D behavior and structure where practical.
- Replace DOS/platform-specific services with SDL3-backed equivalents.
- Keep game data proprietary assets outside git in `game-files/`.
- Use tests as the behavioral bridge from original source to this implementation.

## Current headless verification

The first runnable milestone is a pure C asset/decompression/semantics/model harness. It has no SDL dependency yet and verifies local WL6 data discovery, `MAPHEAD`, `GAMEMAPS`, `VGAHEAD`/`VGADICT`/`VGAGRAPH` Huffman graphics smoke decoding, `STRUCTPIC` picture-table metadata, planar-to-indexed picture surface conversion with indexed-surface descriptors, upload metadata, palette interpolation/fade/shift metadata, gameplay palette-shift state, and palette-shifted upload selection, RGBA expansion, and SDL-free blitting, full `VSWAP` chunk-directory parsing, bounded chunk reads, wall-page metadata/surface conversion, wall texture-column sampling, fixed-height wall strip scaling, tiny wall-strip viewport composition, map-derived wall-hit descriptors, cardinal/fixed-point/DDA/projected ray stepping, view batches, and camera ray tables, and tiny view rendering, sprite shape metadata, sprite post-command metadata, sprite indexed surfaces, scaled sprite rendering, world-space sprite projection/ordering, combined wall+sprite scene rendering, Carmack/RLEW map-plane decompression, first-pass map semantic classification, a minimal `SetupGameLevel`-style runtime model, and runtime sprite-reference selection, sprite surface-cache decoding, and cached runtime-ref sprites feeding combined wall+sprite scene rendering across representative visible refs/camera angles, and multi-map runtime actor coverage for guards, SS, officers, dogs, mutants, bosses, and ghosts, plus player damage/bonus pickup/score/ammo/weapon/key event seams and runtime static pickup/removal hooks, a TransformTile-style visible pickup probe, and a small player motion/collision tick, and use-button door/elevator/pushwall dispatch metadata, and deterministic door open/close progression, and pushwall start/progression metadata, a live runtime solid-plane raycast/render bridge, door-wall render descriptors, runtime door-aware ray hits, door-aware runtime camera wall rendering, and door-aware runtime wall+sprite scene rendering, pushwall wall-page/texture descriptors, and live pushwall scene occlusion.

Run from this directory:

```bash
make test
```

By default the test uses `game-files/base` relative to the repo root. Override with:

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
