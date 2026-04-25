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
