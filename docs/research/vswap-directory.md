# VSWAP Directory Notes

Research/implementation cycle: 2026-04-24 22:41-23:00 CDT  
Scope: VSWAP chunk-directory, bounded reads, wall-page metadata/surface conversion, wall texture-column sampling, fixed-height wall strip scaling, tiny wall-strip viewport composition, map-derived wall-hit descriptors, cardinal/fixed-point/DDA/projected ray stepping, multi-column view batches, camera ray tables, tiny view rendering, metadata-only sprite shape parsing, and sprite indexed-surface decoding, sprite surface-cache decoding, scaled sprite rendering, world-space sprite projection/ordering, and combined wall+sprite scene rendering for WL6 and optional SOD, without committing proprietary chunk bytes.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/ID_PM.C::PML_OpenPageFile`
  - reads `ChunksInFile`, `PMSpriteStart`, and `PMSoundStart` as 16-bit words.
  - reads `ChunksInFile` 32-bit offsets.
  - reads `ChunksInFile` 16-bit lengths.
  - stores offset/length metadata in `PMPages` for later paging.
- `source/original/WOLFSRC/ID_PM.H`
  - `PM_GetSpritePage(v)` maps to `PMSpriteStart + v`.
  - `PM_GetSoundPage(v)` maps to `PMSoundStart + v`.
- `source/original/WOLFSRC/WL_DEF.H`
  - defines `t_compshape` as `leftpix`, `rightpix`, and `dataofs[64]`.
- `source/original/WOLFSRC/WL_DRAW.C::TransformActor` / `TransformTile`
  - translate world points relative to the focal viewpoint, rotate by `viewcos`/`viewsin`, subtract `ACTORSIZE`/object-size from forward distance, reject points closer than `mindist`, and compute `viewx` plus projected height from `heightnumerator / (nx >> 8)`.
- `source/original/WOLFSRC/WL_SCALE.C::ScaleShape`
  - treats each visible sprite column as a list of 6-byte commands: end pixel*2, corrected top/source offset, start pixel*2.
  - a zero end word terminates each column's command list.
  - uses compiled scale-table column widths, screen-edge clipping, transparent post segments, and `wallheight[x] < height` occlusion before drawing sprite columns.

## Implemented seam

Added VSWAP directory APIs/types to `wl_assets`:

- `WL_VSWAP_MAX_CHUNKS`
- `wl_vswap_chunk_kind`
- `wl_vswap_chunk`
- `wl_vswap_directory`
- `wl_read_vswap_directory(...)`

The parser:

- reads the full offsets and lengths tables;
- classifies chunks as wall/sprite/sound/sparse based on `PMSpriteStart` and `PMSoundStart`;
- validates range ordering, table/data bounds, and max chunk end;
- records aggregate wall/sprite/sound/sparse counts;
- provides a bounded chunk read helper for tests and later decoders;
- decodes safe wall/sprite shape metadata without retaining proprietary pixels/posts;
- converts raw column-major wall pages into caller-owned row-major indexed surfaces for renderer/raycaster seams;
- samples wall texture columns using the original `texture` byte-offset model and cross-checks against row-major surfaces;
- scales sampled 64-byte wall columns into caller-owned indexed surfaces with the original compiled-scaler source-pixel run model;
- composes deterministic wall-strip viewport smoke tests without SDL or committed pixel bytes;
- derives wall-hit descriptors from decoded map-plane tiles using the original horizontal/vertical wall-page mapping;
- walks cardinal map rays to the first solid wall and emits wall-hit descriptors for the viewport seam;
- derives texture columns from 16.16 fixed-point ray origins for cardinal rays;
- steps arbitrary 16.16 fixed-point ray vectors through map grid intersections with a DDA helper;
- projects DDA hit distances into deterministic wall-column heights using the original `CalcProjection`/`CalcHeight` constants;
- batches projected ray columns into ordered screen-column hit descriptors for future view rendering;
- builds half-pixel-sampled camera ray directions from fixed-point forward and camera-plane vectors;
- renders tiny camera wall views by composing camera rays, projected DDA batches, wall-page lookup, and strip rendering;
- validates/counts sprite post-command streams without retaining pixel data;
- decodes sprite post streams into caller-owned transparent indexed surfaces for future sprite rendering, including 16-bit wraparound of corrected top/source offsets used by some original sprite posts;
- decodes caller-provided VSWAP sprite chunk-index lists into contiguous caller-owned indexed-surface caches;
- scales transparent indexed sprites into caller-owned viewports with source-run distribution, clipping, and optional wall-height occlusion;
- projects world-space sprite centers into screen-x/height descriptors and sorts projected sprites far-to-near before composition;
- combines camera wall rendering and projected sprite rendering into a single deterministic indexed scene seam, now exercised with cached surfaces selected from runtime scene refs;
- does not copy or commit any proprietary chunk bytes.

## WL6 committed assertions

`game-files/base/VSWAP.WL6`:

- file size: `1,544,376`
- chunks: `663`
- `PMSpriteStart = 106`
- `PMSoundStart = 542`
- directory table end/data-start minimum: `3984`
- max chunk end: `1,544,376`
- walls/sprites/sounds/sparse: `106 / 436 / 121 / 0`
- first wall chunk: index `0`, offset `4096`, length `4096`
- last wall chunk: index `105`, offset `434176`, length `4096`
- first sprite chunk: index `106`, offset `438272`, length `1306`
- last pre-sound sprite sample: index `541`, offset `1139200`, length `650`
- first sound chunk: index `542`, offset `1140224`, length `4096`
- final chunk: index `662`, offset `1544192`, length `184`
- read smoke hashes: chunk `0` `0x98d020a5`, chunk `106` `0xbf4fcd99`, chunk `542` `0xaee73350`, chunk `662` `0xfba68c74`
- wall metadata for chunk `0`: `64x64`, `64` columns, colors `7..31`, `18` unique colors, row-major indexed hash `0x8fe4d8ff`, sampled column hashes `0xc77d483d`, `0x272b5483`, `0x2fbb79bb`, `0x19c55a4e`, scaled-strip canvas hashes `0xceb8a051`, `0xf25f51d9`
- wall metadata for chunk `63`: colors `26..223`, `31` unique colors, row-major indexed hash `0x5b4d4c38`, sampled column hash `0x8a859220`, combined scaled-strip/viewport canvas hash `0x0b200118`; map-derived viewport hash `0x7ffb21c0`; cardinal/fixed-ray viewport hash `0xa4c9e6e1`, DDA mixed-ray viewport hash `0xae40b70c`, projected-ray viewport hash `0xd48f2f6d`, batched-view viewport hash `0x7209a9ed`, camera-ray viewport hash `0x7320f695`, tiny-view render hash `0xfad71929`
- wall metadata for chunk `105`: colors `0..31`, `11` unique colors, row-major indexed hash `0x66874cf5`
- sprite metadata for chunk `106`: `64x64`, left/right pixels `4..58`, `55` visible columns, first/last column offsets `800/1298`
- sprite post metadata for chunk `106`: `66` posts, `55` column terminators, `1..2` posts/column, span range `2..40`, source-offset range `108..782`, total post span `1372`, transparent indexed-surface hash `0x918ed728`, non-transparent pixels `614`; scaled-sprite viewport hashes `0x3f753ac8`, occluded `0xaa7c2838`, clipped `0x6ff0f5c8`; world-projected sprite descriptors `(view_x,height,distance)=(39,42,0x51700)/(46,30,0x71700)`, sorted render hash `0x819b1035`, combined wall+sprite scene hash `0x1e4a8264`; sprite-ref surface-cache hashes `0x38769770`, `0xbd6176ba`, `0x0fe580fa`, `0xa875d685`, `0x63f7eba2`, combined cache hash `0x4a8eb8db`; broader runtime-ref scene hashes `0xb92e568b` and `0x4668f191`; multi-map enemy scene-ref hashes `0xab87ed41`, `0x89b8f3c0`, `0xc090c2df`, and boss-map hashes `0xb2dab28b`, `0x713b74e4`, `0x7a3bd88c`, and `0x2b8bf08d`, plus expanded mixed boss/ghost secret-map hash `0x59f0ca20`, and ghost-map hash `0xe03fdb45`
- sprite post metadata for chunk `107`: `85` posts, `62` column terminators, `1..3` posts/column, max span `36`, source-offset range `113..904`, total post span `1586`, transparent indexed-surface hash `0x88a2d1b4`, non-transparent pixels `384`

## Optional SOD committed assertions

When `game-files/base/m1/VSWAP.SOD` is present:

- file size: `1,616,544`
- chunks: `666`
- `PMSpriteStart = 134`
- `PMSoundStart = 555`
- directory table end/data-start minimum: `4002`
- max chunk end: `1,616,544`
- walls/sprites/sounds/sparse: `134 / 421 / 111 / 0`
- last wall chunk: index `133`, offset `548864`, length `4096`
- first sprite chunk: index `134`, offset `552960`, length `1306`
- first sound chunk: index `555`, offset `1233408`, length `4096`
- final chunk: index `665`, offset `1616384`, length `160`
- read smoke hashes: chunk `0` `0x98d020a5`, chunk `134` `0xbf4fcd99`, chunk `555` `0xaee73350`, chunk `665` `0xbb53ed59`
- wall metadata for chunk `0`: `64x64`, `64` columns, colors `7..31`, `18` unique colors, row-major indexed hash `0x8fe4d8ff`, sampled column hash `0x2fbb79bb`, scaled-strip canvas hash `0x78547277`
- wall metadata for chunk `105`: colors `0..237`, `26` unique colors, row-major indexed hash `0x997d475d`, sampled column hashes `0xd61f9cbd`, `0x3e5f4efd`, combined scaled-strip/viewport canvas hash `0x60ddb236`
- wall metadata for chunk `133`: colors `0..31`, `11` unique colors, row-major indexed hash `0x66874cf5`
- sprite metadata for chunk `134`: `64x64`, left/right pixels `4..58`, `55` visible columns, first/last column offsets `800/1298`
- sprite post metadata for chunk `134`: `66` posts, `55` column terminators, `1..2` posts/column, source-offset range `108..782`, total post span `1372`
- sprite post metadata for chunk `135`: `85` posts, `62` column terminators, max `3` posts/column, max span `36`, max source offset `904`, total post span `1586`

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
asset/decompression/semantics/model/vswap/runtime-live-ref-scene tests passed for game-files/base
```

## Cycle update: chunk reads and shape metadata

Added `wl_read_vswap_chunk`, a bounded read helper that validates chunk index, sparse entries, output buffer size, and directory/file bounds before reading chunk bytes into caller-provided memory. Tests assert lengths and stable FNV-1a hashes for representative wall, sprite, sound, and final chunks without committing bytes.

Added `wl_decode_vswap_shape_metadata` for safe metadata-only interpretation of representative wall/sprite chunks. Wall chunks assert the canonical `64x64` raw page shape; sprite chunks assert `t_compshape`-style left/right bounds, packed column-offset table metadata, and post-command list counts/ranges.

Added `wl_decode_wall_page_metadata`, `wl_decode_wall_page_to_indexed`, and `wl_decode_wall_page_surface` for raw VSWAP wall pages. These functions preserve the original column-major `PM_GetPage(wallpic) + texture` addressing model while providing row-major indexed surfaces for future SDL3 texture/raycaster seams. Added `wl_sample_wall_page_column` and `wl_sample_indexed_surface_column` so raycaster-oriented code can sample a canonical 64-byte texture column and verify it matches the row-major surface view. Added `wl_scale_wall_column_to_surface`, a pure C fixed-height wall strip scaler that mirrors the original `BuildCompScale` source-pixel run distribution while writing to linear indexed surfaces. Added `wl_render_wall_strip_viewport`, a tiny SDL-free composition seam that renders an ordered set of wall-strip commands into a caller-owned indexed viewport. Added `wl_build_map_wall_hit` and `wl_wall_hit_to_strip` to turn decoded map-plane wall tiles into horizontal/vertical page-index + texture-offset descriptors before viewport composition. Added `wl_cast_cardinal_wall_ray`, a first map stepping helper that walks north/east/south/west from a tile and emits the first solid-wall hit descriptor. Added `wl_cast_fixed_cardinal_wall_ray`, which accepts 16.16 world origins and derives original-style texture offsets from the perpendicular intercept coordinate. Added `wl_cast_fixed_wall_ray`, which steps arbitrary 16.16 direction vectors across vertical/horizontal map-grid crossings, derives the hit-side texture column from the fixed-point intercept coordinate, and feeds the existing wall-hit/viewport seam. Added `wl_project_wall_height` and `wl_cast_projected_wall_ray`, which carry fixed DDA hit distance into a deterministic projected wall height for SDL-free column rendering. Added `wl_cast_projected_wall_ray_batch`, which validates and emits an ordered run of screen-column wall-hit descriptors from caller-provided 16.16 ray vectors. Added `wl_build_camera_ray_directions`, which generates half-pixel-centered fixed-point ray vectors from a forward vector and camera-plane vector before feeding projected batches. Added `wl_render_camera_wall_view`, which builds camera rays, casts projected batches, maps hit page indices to caller-supplied wall pages, and renders strips into an indexed viewport.

## Cycle update: sprite post metadata

Extended sprite metadata decoding to walk each visible column's post-command stream, validate start/end pixel*2 ranges and zero terminators, and record aggregate post counts, post-span ranges, source-offset ranges, and posts-per-column bounds. Added `wl_decode_sprite_shape_to_indexed` and `wl_decode_sprite_shape_surface`, which decode the same post streams into caller-owned transparent `64x64` indexed surfaces, including wrapped corrected-top offsets seen in some original shapes. Added `wl_render_scaled_sprite`, a pure C viewport compositor that scales those transparent surfaces, clips to the destination, and honors optional wall-height occlusion like the original sprite scaler. Added `wl_project_world_sprite` and `wl_sort_projected_sprites_far_to_near`, which carry world-space sprite centers into original-style screen-x/height descriptors and deterministic far-to-near draw order. Added `wl_decode_vswap_sprite_surface_cache`, which decodes a caller-provided list of sprite chunk indices into contiguous caller-owned indexed surfaces for render use. Added `wl_render_camera_scene_view`, which renders camera wall columns, records a wall-height occlusion buffer, projects/sorts sprites, and composites transparent sprite surfaces into the same indexed viewport. This gives the renderer path deterministic sprite layout/composition/projection/scene oracles without storing or committing proprietary pixel bytes.

## Cycle update: runtime-ref cached scene rendering

The headless scene test now selects visible WL6 map-0 runtime sprite refs `113` and `114`, decodes their VSWAP chunks through `wl_decode_vswap_sprite_surface_cache`, and passes the resulting caller-owned surfaces plus ref world coordinates/source ids directly into `wl_render_camera_scene_view`. Assertions cover per-surface hashes `0x442facd4` and `0xd363bf0c`, combined cache hash `0xd53b06f5`, sorted/projected draw order (`source_index` `26` then `16`), screen-x/height descriptors, and final indexed scene hash `0x61f7f78b`. This keeps the renderer bridge deterministic and asset-byte-free while proving the runtime-ref -> cache -> combined-scene path.

## Cycle update: broader runtime-ref scene coverage

Expanded the runtime-ref scene smoke test from two visible refs to five WL6 map-0 refs (`110`, `111`, `113`, `114`, `115`). The test decodes their VSWAP chunks through the sprite surface cache, verifies per-surface hashes plus combined cache hash `0x61a879ca`, feeds their runtime world coordinates/source ids into `wl_render_camera_scene_view`, and asserts sorted projected source order `21,11,26,16,16` with final indexed scene hash `0xb92e568b`. The same cached runtime refs are also rendered through a northeast camera vector, asserting shifted view-x/height/distance descriptors and final scene hash `0x4668f191`. This gives broader deterministic coverage for overlapping static refs, duplicate chunks, clipping, and far-to-near composition without storing proprietary sprite bytes.

## Next step

Add more map runtime-scene coverage, connect renderer seams to future live gameplay events, or add a small SDL3 presentation boundary. Keep assertions to decoded metadata and stable hashes rather than committing chunk bytes.


## Cycle update: multi-map enemy scene refs

The VSWAP sprite-ref seam now receives additional runtime actor classes from maps beyond WL6 map 0. Officer, SS, and mutant actors map to their original starting stand/path sprite indices before chunk lookup, so `wl_collect_scene_sprite_refs` can describe renderer inputs for `Wolf1 Map2`, `Wolf2 Map1`, and `Wolf3 Map1` without decoding or committing proprietary bytes. Tests assert scene-ref descriptor hashes instead of stored sprite data.


## Cycle update: boss-map scene refs

Boss runtime actors now flow through `wl_collect_scene_sprite_refs` with original starting sprite indices for known WL6 boss tiles. The test coverage now includes `Wolf2 Boss`, `Wolf3 Boss`, `Wolf4 Boss`, `Wolf6 Boss`, and `Wolf6 Secret`, asserting scene-ref descriptor hashes instead of committing any decoded boss sprite bytes. `Wolf1 Boss`/`Wolf5 Boss` remain useful follow-ups because their unsupported info tiles still need a small classification pass before zero-unknown model assertions.


## Cycle update: ghost-map scene refs

Ghost runtime actors now flow through `wl_collect_scene_sprite_refs` with original starting sprite indices for Blinky, Clyde, Pinky, and Inky. The `Wolf3 Secret` test asserts scene-ref descriptor hashes instead of committing any decoded ghost sprite bytes.


## Cycle update: live solid-plane raycast bridge

Runtime tilemap changes from door and pushwall seams can now feed the existing raycaster through `wl_build_runtime_solid_plane`. Headless tests assert that closed doors and moving pushwalls alter cardinal ray hits before a full animated door/pushwall renderer exists. Door centers currently use a caller-selected placeholder wall tile for solid-ray coverage; textured door rendering remains a later presentation/raycast specialization.


## Cycle update: live runtime wall rendering

The runtime solid-plane bridge now has a direct wall-view render wrapper, `wl_render_runtime_camera_wall_view`. It reuses existing decoded VSWAP wall-page buffers and asserts deterministic canvas hashes for closed-door versus open-door live tilemap state. Door centers still use a placeholder solid wall tile until textured/animated door rendering is specialized, but the renderer path now observes live runtime collision state instead of only immutable map wall planes.


## Cycle update: door wall descriptors

Door pages now have a deterministic headless descriptor seam. `wl_build_door_wall_hit` maps `PMSpriteStart - 8` into normal, locked, and elevator door wall pages and applies `doorposition` to the sampled texture column before the existing wall-strip renderer consumes the result. The current test reads only local VSWAP door pages, asserts page/texture metadata, and verifies a locked-door strip canvas hash rather than committing any proprietary bytes.


## Cycle update: runtime door-aware ray hits

The raycaster now has a live runtime model entry point, `wl_cast_runtime_fixed_wall_ray`, which distinguishes closed door centers from ordinary walls and emits door page/texture descriptors using the existing door-wall seam. The same helper also carries moving pushwall wall ids through the ray-hit descriptor. This closes the gap between live door/pushwall state and renderer-facing projected ray metadata while remaining display-free.


## Cycle update: door-aware runtime camera rendering

Runtime door-aware ray hits now drive a complete headless camera wall render path. `wl_render_runtime_door_camera_wall_view` uses live model hits to choose real wall/door VSWAP page indices and render strips, so closed/open door state changes the resulting indexed canvas hash without SDL or committed pixels. This supersedes the earlier placeholder solid-plane render bridge for door-aware camera tests while leaving it available as a simpler collision/render smoke seam.


## Cycle update: door-aware runtime scene rendering

The live runtime renderer now has a combined wall+sprite scene entry point. `wl_render_runtime_door_camera_scene_view` renders door-aware wall strips, records live occlusion heights, then projects and composites sprite surfaces. The test keeps all decoded VSWAP pixels local and commits only the resulting closed/open scene hashes for a doorway sprite case.


## Cycle update: pushwall render descriptors

Moving pushwall tiles now have a dedicated wall-hit descriptor path. `wl_build_pushwall_wall_hit` maps the original moving tile marker into wall page indices and original-style texture-column orientation, and the live runtime DDA ray helper uses it for `0xc0` tilemap entries. This keeps pushwall rendering deterministic and SDL-free while committing only page/texture metadata assertions.


## Cycle update: live pushwall scene occlusion

Added deterministic renderer coverage for moving pushwalls using real local VSWAP wall page data. The live scene test maps a moving pushwall marker to wall page `73`, checks that it occludes a sprite behind it, then clears the marker and confirms the sprite-visible scene hash changes. Assertions remain metadata/hash-only and do not commit proprietary wall or sprite bytes.


## Cycle update: pushwall sub-tile render offsets

Moving pushwall render coverage now includes the `pwallpos` sub-tile offset. Runtime rays still select VSWAP wall page/texture metadata from the moving tile marker, but their emitted distance/projected height now shifts with `pwallpos`, producing a distinct deterministic scene hash for a partially moved pushwall. Assertions remain hash/metadata-only; decoded VSWAP wall bytes stay local.


## Cycle update: live runtime-ref scene rendering

The VSWAP sprite-cache path is now exercised through live door-aware model rendering with real runtime refs. Five decoded local sprite surfaces are composited behind the mutable `wl_game_model::tilemap`; a closed door uses VSWAP door page `99`, while clearing that live tile lets the scene render against the later wall. Tests commit only emitted page metadata and scene hashes (`0x21495346` closed, `0x2e4660d2` open).


## Cycle update: live tick scene rendering

The live wall+sprite renderer now has coverage driven by `wl_step_live_tick` rather than manual tilemap mutation. A keyed use tick opens a door and clears the live tilemap center, then `wl_render_runtime_door_camera_scene_view` renders the resulting open doorway using local VSWAP wall/sprite pages. The assertion compares the tick-driven scene hash to the existing open-door scene hash, keeping verification metadata-only.


## Cycle update: live tick pushwall scene rendering

Moving-pushwall scene rendering is now driven by `wl_step_live_tick`, not only manual tilemap setup. A use-button tick starts and advances a pushwall to a live `0xc0` marker; the runtime scene renderer then consumes local VSWAP wall page `73` and produces the same committed moving-pushwall occlusion hash. No decoded wall/sprite bytes are committed.


## Cycle update: live tick static scene removal

Runtime static pickup removal is now covered through the VSWAP sprite-cache/render path. The headless test decodes only the locally referenced bonus-static sprite chunk, renders it while active, then runs `wl_step_live_tick` so `wl_collect_scene_sprite_refs` omits the deactivated static and the scene hash changes. Assertions remain metadata/hash-only; no decoded proprietary sprite bytes are committed.


## Cycle update: live actor drop scene rendering

The VSWAP sprite-cache path now receives statics spawned by live actor kill/drop state, not only map-authored statics. A guard killed by `wl_step_live_actor_damage_tick` creates a clip drop whose static type maps to source sprite `28` / chunk `134`; the test decodes that chunk locally and renders the drop in the live door-aware scene path, committing only descriptor assertions and scene hash `0x707dbe4e`.


## Cycle update: actor death-state sprite metadata

Death animation progression now emits VSWAP sprite source indices for killed actors. The new seam maps representative original `WL_ACT2.C` death chains to source-index/tic metadata for guards, dogs, SS, mutants, officers, and bosses, preparing actor death frames for future sprite-cache decoding and scene rendering without committing decoded sprite bytes.


## Cycle update: actor death-state scene rendering

Death-state sprite source indices now drive VSWAP sprite-cache decoding. A killed guard death frame maps to source sprite `91` / VSWAP chunk `197`, decodes locally into a temporary indexed surface, and renders through the live scene path with committed scene hash `0x2e8b4819`.


## Cycle update: full live combat death refs

Live full-combat results now carry death-frame VSWAP metadata directly. The same-frame killed guard path returns a death scene ref for source sprite `91` / chunk `197`, tying actor kill/drop gameplay output to the sprite-cache/render boundary without committing decoded sprite bytes.


## Cycle update: full combat death-ref scene rendering

The VSWAP cache path now consumes death refs emitted directly by full live combat. A same-frame guard kill returns source sprite `91` / chunk `197`; the test decodes that chunk locally and renders it with stable scene hash `0x2e8b4819`, committing only metadata and hashes.


## Cycle update: actor death final-frame refs

The runtime actor-ref path can now carry final death-frame VSWAP metadata. Applying a finished guard death state to an actor slot causes `wl_collect_scene_sprite_refs` to emit source sprite `95` / chunk `201` for that same model index, without storing decoded sprite bytes.


## Cycle update: live actor death tick

Death-frame VSWAP metadata now updates through a live tick boundary. The guard death tick emits source `92` / chunk `198` during progression and final source `95` / chunk `201` at completion, after which `wl_collect_scene_sprite_refs` carries the corpse frame from the runtime actor slot.


## Cycle update: full combat death tick orchestration

The combined full-combat/death-tick path now carries active death VSWAP metadata through the same frame result as combat state. The guard test finalizes a full-combat death to source `95` / chunk `201` on actor model index `9`, preserving metadata-only verification.


## Cycle update: full combat death-tick final scene

Full-combat/death-tick output now consumes final death-frame VSWAP data in the scene renderer. The final guard corpse ref uses source `95` / chunk `201` and renders with committed hash `0x81c10dcf`, keeping decoded sprite bytes local.


## Cycle update: broadened actor death final refs

Final death-frame VSWAP metadata is now asserted for more enemy classes through runtime scene refs: officer source/chunk `284/390`, SS `183/289`, dog `134/240`, mutant `233/339`, and boss `303/409`. Verification remains metadata-only; decoded sprite bytes stay local.


## Cycle update: patrol actor scene refs

Patrol stepping now verifies renderer-facing VSWAP metadata remains stable while actor world coordinates change. The patrol guard ref keeps source/chunk `58/164` through `wl_collect_scene_sprite_refs`, proving movement can update scene placement without reclassifying the sprite asset.


## Cycle update: patrol actor scene render

Patrol actor source/chunk metadata (`58/164`) now feeds the scene render path, not just ref collection. The renderer test consumes local sprite/cache metadata and asserts projection behavior without committing proprietary sprite bytes.


## Cycle update: live AI patrol scene refs

The VSWAP metadata path now observes patrol movement from the live actor AI frame wrapper. A moved patrol guard remains source/chunk `58/164` after live AI ticking, proving the future sprite-cache/upload path can consume AI-updated actor refs without decoded asset bytes in git.


## Cycle update: live AI patrol rendering

The VSWAP sprite-cache path now consumes refs produced after live AI patrol movement. The moved guard source/chunk `58/164` is decoded locally and rendered with hash `0x6ee1f8bf`; committed assertions remain metadata/hash-only.


## Cycle update: patrol fine-position refs

Actor sprite metadata remains stable while patrol placement advances at sub-tile precision. The moved guard keeps source/chunk `58/164`; only scene-ref world coordinates change, so decoded VSWAP bytes remain local.


## Cycle update: live AI fine-position patrol rendering

Fine-position live AI patrol refs now consume the VSWAP sprite-cache path. The guard remains source/chunk `58/164`, while sub-tile world coordinates change the rendered scene hash to `0xcf61b07b`; decoded bytes stay local.


## Cycle update: patrol remainder accumulation

Patrol sprite metadata remains stable while accumulated sub-tile motion changes placement. The guard keeps source/chunk `58/164`; only fine world coordinates and later tile centers change.


## Cycle update: live AI remainder rendering

The VSWAP sprite path now sees accumulated live-AI patrol placement across frames. The guard remains source/chunk `58/164`; only the world coordinates and rendered hashes change (`0xcf61b07b` then `0x6ee1f8bf`).


## Cycle update: live AI dog fine-position rendering

The live AI fine-position VSWAP path now validates dog patrol sprite metadata. Source/chunk `99/205` decodes locally and renders with hash `0x08ab64f0`; no decoded sprite bytes are committed.


## Cycle update: live AI officer fine-position rendering

The live AI fine-position VSWAP path now validates officer patrol sprite metadata. Source/chunk `246/352` decodes locally and renders with hash `0xa6544334`; only metadata and hashes are committed.


## Cycle update: live AI SS fine-position rendering

The live AI fine-position VSWAP path now validates SS patrol sprite metadata. Source/chunk `146/252` decodes locally and renders with hash `0x0b6fe30b`; only metadata and hashes are committed.


## Cycle update: live AI mutant fine-position rendering

The live AI fine-position VSWAP path now validates mutant patrol sprite metadata. Source/chunk `195/301` decodes locally and renders with hash `0x96655cea`; only metadata and hashes are committed.


## Cycle update: live AI boss fine-position rendering

The live AI fine-position VSWAP path now validates boss sprite metadata. Hans source/chunk `296/402` decodes locally and renders with hash `0x731d6cb3`; only metadata and hashes are committed.


## Cycle update: chase direction selection seam

VSWAP sprite metadata remains unchanged while live actor AI gains a deterministic chase-direction decision seam. This prepares chase movement to feed existing sprite-ref/render paths without committing decoded assets.


## Cycle update: chase actor step seam

Chase-stepped actor refs continue to select existing VSWAP walk sprite metadata (`58/164` for the guard test) while placement changes through runtime state rather than committed asset bytes.


## Cycle update: chase tic fine-position seam

Chase fine-position movement changes actor placement while keeping existing VSWAP walk sprite metadata. Tests continue to assert metadata/hashes only, with decoded bytes local.


## Cycle update: chase fine-position rendering

The chase fine-position render path validates guard walk sprite metadata `58/164` through local VSWAP decode and stable scene hash `0xa71311c2`; no decoded bytes are committed.


## Cycle update: live AI chase fine-position rendering

Live AI chase refs now consume the same VSWAP sprite-cache path as patrol refs. The guard remains source/chunk `58/164`; rendered hash `0xa71311c2` verifies placement without committing decoded bytes.


## Cycle update: live AI chase remainder rendering

The live AI chase remainder path keeps guard source/chunk `58/164` stable while placement changes across frames. Render hashes `0xa71311c2` then `0x4a4c3e4f` verify metadata-only placement changes.


## Cycle update: live AI chase combat bridge

Chase movement still preserves VSWAP guard metadata while the moved actor feeds combat. Asset assertions remain metadata/hash-only; combat validation uses deterministic rolls.


## Cycle update: live AI dog chase bite bridge

The new dog chase-combat check remains metadata/state-only and does not commit decoded VSWAP bytes; it broadens behavior coverage around the same runtime actor refs.


## Cycle update: live AI shooter-class chase combat

Shooter-class chase combat remains metadata/state-only and does not commit VSWAP bytes. The coverage complements existing renderer-facing class refs with combat behavior checks.


## Cycle update: live AI chase full-combat bridge

The chase full-combat bridge verifies death sprite source/chunk `91/197` from the moved guard while continuing to avoid committing decoded VSWAP bytes.


## Cycle update: live AI chase full-combat render

The chase full-combat render path decodes only local VSWAP sprite chunks for verification and commits metadata/hash assertions: death source/chunk `91/197`, drop `28/134`, scene hash `0x4a76f09a`.


## Cycle update: live AI chase death-final render

The final chase death render verifies VSWAP metadata only: initial death `91/197`, final corpse `95/201`, drop `28/134`, and scene hash `0x8a2741bf`.


## Cycle update: live AI chase death-transition render

The VSWAP-backed chase death transition now asserts both sprite phases: initial death `91/197` and final corpse `95/201`, with the same drop `28/134`, using hashes only.


## Cycle update: shooter-class chase death-final metadata

Chase death-final metadata now asserts final corpse chunks for officer `284/390`, SS `183/289`, mutant `233/339`, and boss `303/409` without committing decoded VSWAP bytes.


## Cycle update: shooter-class chase death-final render hashes

The class death render checks decode local VSWAP chunks only for verification: officer `284/390`, SS `183/289`, mutant `233/339`, boss `303/409`, plus their drops, with hash-only assertions.


## Cycle update: dog chase death-final render

The dog chase final render verifies VSWAP metadata/hash only: final corpse source/chunk `134/240`, no committed decoded bytes, scene hash `0x92ff40dd`.


## Cycle update: headless presentation frame descriptor

VSWAP-backed render output now crosses a presentation descriptor seam: decoded local verification data remains hash-only while future SDL3 can consume texture upload metadata.


## Cycle update: palette-shifted present frame descriptor

The VSWAP-backed frame descriptor now separates indexed sprite pixels from palette upload state, preserving metadata/hash-only checks for future SDL3 texture uploads.


## Cycle update: combat palette present frame descriptors

VSWAP-backed frame presentation now verifies palette-only upload changes for red and white shifts without committing decoded asset bytes.
