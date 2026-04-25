# Autopilot State

Status: active

## Current Phase

Runtime sprite refs feed cached VSWAP sprite surfaces into the combined wall+sprite scene renderer with broader visible-ref/camera-angle coverage, additional maps cover officer/SS/mutant/boss/ghost runtime actor refs, and palette fade/shift generation, state, shifted upload selection, player gameplay/bonus pickup events, and runtime static pickup/removal hooks, a TransformTile-style visible pickup probe, and a small player motion/collision tick, and use-button door/elevator/pushwall dispatch metadata and deterministic door open/close progression, and pushwall start/progression metadata, a live runtime solid-plane raycast/render bridge, door-wall render descriptors, runtime door-aware ray hits, and door-aware runtime camera wall rendering, and door-aware runtime wall+sprite scene rendering, pushwall wall-page/texture descriptors, and live pushwall scene occlusion, and pushwall sub-tile distance/height, and live runtime-ref door scene coverage, and a headless live gameplay tick and live tick palette-upload selection and live tick scene rendering and live tick pushwall scene rendering and live tick static pickup scene removal and actor bite damage, actor shooting damage, projectile damage, live projectile tick damage, live actor attack tick damage, and combined live combat tick damage, live combat palette-upload selection, and actor damage/kill state, actor drop static spawning, live actor damage/drop spawning tick integration, live-spawned actor drop scene rendering, a full live combat tick combining outgoing actor damage/drop with incoming actor/projectile damage, deterministic actor death-state progression, death-state actor scene rendering, and live full-combat death-state/ref output, full-combat death-ref scene rendering, final death-frame replacement in runtime actor refs, live actor death animation ticking/finalization, and full-combat orchestration with active death ticking, and combined full-combat/death-tick final scene rendering, and broadened actor death final-frame refs for officer/SS/dog/mutant/boss, and expanded WL6 boss/secret map runtime scene-ref coverage, are covered headlessly. Next phase should deepen actor AI progression, add more runtime actor state progression, fill remaining unknown boss info-tile classifications, or add a small SDL3 presentation boundary when SDL3 is available.

## Latest Verified Milestone

- Repo exists under `~/.openclaw/tmp/Wolfenstein 3D port`.
- Original id Software Wolfenstein 3D source is available at `source/original/` as a submodule.
- Local purchased game data is present under `game-files/base/` and ignored by git.
- Autopilot operating rules are documented in `docs/AUTOPILOT.md`.
- `source/original/` was inspected but not modified.
- `docs/research/source-inventory.md` records the first source/data inventory and practical test-driven porting strategy.
- `source/modern-c-sdl3` now contains the first pure C asset/decompression harness (`wl_assets`) plus `make test` headless verification.
- `docs/research/asset-metadata-harness.md` records the initial metadata seam, assertions, and verification output.
- `docs/research/map-decompression.md` records the Carmack/RLEW implementation seam, hash/count assertions, and verification output.
- `docs/research/map-semantics.md` records original source references and WL6 map 0 semantic-count assertions.
- `docs/research/runtime-map-model.md` records the pure C runtime model seam, door-area connectivity descriptors, renderer-facing scene sprite references, descriptor assertions, and verification output.
- `docs/research/vswap-directory.md` records full VSWAP chunk-directory parsing, bounded chunk-read hashes, wall-page metadata/surface/column-sampler/scaler/viewport/map-hit/cardinal/fixed/DDA/projected/view-batch/camera-ray/tiny-view assertions, sprite shape metadata assertions, sprite post-command metadata/indexed-surface/surface-cache/scaled-render/world-projection/scene-render/runtime-ref/broad-scene assertions, range/count assertions, and verification output.
- `docs/research/graphics-huffman.md` records VGAHEAD/VGADICT/VGAGRAPH parsing, pure C Huffman expansion, STRUCTPIC picture-table metadata, planar-to-indexed surface conversion, renderer-facing indexed-surface descriptors, upload metadata/RGBA expansion, palette fade/shift metadata, state, and shifted upload selection, SDL-free indexed blitting, WL6/SOD graphics chunk smoke assertions, and verification output.

## Verified Findings

### Source Architecture

- Original code is a Borland/DOS C codebase with inline assembly and separate `.ASM` modules.
- Reusable id subsystems are organized around:
  - `ID_CA.*` cache/data file loading and Huffman/Carmack/RLEW decompression.
  - `ID_MM.*` segmented DOS memory manager.
  - `ID_PM.*` VSWAP page manager with EMS/XMS/main-memory paging.
  - `ID_VL.*` / `ID_VH.*` VGA/palette/blit helpers.
  - `ID_IN.*`, `ID_SD.*`, `ID_US.*` input, sound, and UI helpers.
- Wolf-specific game files are organized around:
  - `WL_MAIN.C` startup/main loop.
  - `WL_MENU.C` extension/SKU detection and menus.
  - `WL_GAME.C` map loading and level setup.
  - `WL_ACT*.C`, `WL_AGENT.C`, `WL_STATE.C` actor/player behavior.
  - `WL_DRAW.C`, `WL_PLAY.C`, `WL_SCALE.C`, `CONTIGSC.C` rendering/raycast/scaling.
- `WL_MAIN.C::InitGame` initializes `MM`, video, input, page manager, sound, cache manager, and UI in that order, then builds lookup/render tables.
- `WL_MENU.C::CheckForEpisodes` chooses data extension (`WL6`, `WL3`, `WL1`, `SOD`, `SDM`, etc.) and appends it to global file-name stems.

### Data Files

Verified local WL6 data:

- `MAPHEAD.WL6`: 402 bytes; RLEW tag `0xabcd`; 100 map offset slots. Original `ID_CA.H` defines `NUMMAPS` as 60 for WL6 gameplay, so read the table generically but constrain first gameplay tests to the original playable map count.
- `GAMEMAPS.WL6`: 150,652 bytes; map 0 offset 2250; map 0 name `Wolf1 Map1`; dimensions 64x64; plane starts `(11, 1445, 2240)`; plane lengths `(1434, 795, 10)`.
- `VSWAP.WL6`: 1,544,376 bytes; `ChunksInFile=663`, `PMSpriteStart=106`, `PMSoundStart=542`; first offsets `4096,8192,12288,16384,20480`.
- `VGAHEAD.WL6`: 450 bytes; 3-byte graphics offsets.
- `VGADICT.WL6`: 1024 bytes.
- `VGAGRAPH.WL6`: 275,774 bytes.
- `AUDIOHED.WL6`: 1,156 bytes; first offsets `(0, 15, 28, 44, 102)`.
- `AUDIOT.WL6`: 320,209 bytes.

Verified optional SOD data under `game-files/base/m1`:

- `MAPHEAD.SOD`: 402 bytes; RLEW tag `0xabcd`; 100 map offset slots.
- `GAMEMAPS.SOD`: first map offset 2097; map 0 name `Tunnels 1`; dimensions 64x64.
- `VSWAP.SOD`: `ChunksInFile=666`, `PMSpriteStart=134`, `PMSoundStart=555`.

### Test Strategy

- Do not try to compile the full original DOS program as the first oracle; it is blocked by Borland headers, segmented memory, inline asm, DOS interrupts, VGA hardware, and EMS/XMS assumptions.
- Prefer pure modern C modules for original data transformations, with tests against local data-derived metadata/hashes.
- Best direct comparison targets: `CAL_CarmackExpand`, `CA_RLEWexpand`, `CAL_HuffExpand`, map/VSWAP/header parsing, fixed-point/table generation, and later spawn classification.
- Low-value exact comparisons: DOS memory placement, VGA planar side effects, compiled x86 scalers, hardware audio writes, and interrupt timing.

## Current Strategy

Target WL6 first, while keeping parsers flexible enough to validate SOD as optional secondary data. Build a deterministic asset/decompression foundation before adding SDL3 video/input/audio. Treat SDL3 as the platform boundary, not the first dependency.

Use tests as the bridge from the original code to modern C:

1. Asset locator + required file metadata test.
2. `MAPHEAD`/`GAMEMAPS` parser test.
3. Carmack + RLEW map-plane decompression test. **Done for WL6 map 0.**
4. Decoded map semantic characterization: wall/object/player-start/actor/static classifications. **Done for WL6 map 0.**
5. Minimal SetupGameLevel-style runtime map model: tilemap, doors, statics, player spawn, and difficulty-filtered actor descriptors. **Done for WL6 map 0.**
6. Door-area connectivity model. **Done for WL6 map 0.**
7. `VSWAP` parser/chunk descriptor test. **Done for WL6 and optional SOD.**
8. VSWAP chunk read smoke tests. **Done for WL6 and optional SOD.**
9. VSWAP wall/sprite shape metadata decoding. **Initial representative WL6/SOD metadata done.**
10. VSWAP sprite post-command metadata. **Done for representative WL6/SOD sprites.**
10a. VSWAP sprite transparent indexed surfaces. **Done for representative WL6 sprites.**
11. `VGAHEAD`/`VGADICT`/`VGAGRAPH` Huffman smoke test. **Done for representative WL6/SOD chunks.**
12. `STRUCTPIC` picture metadata interpretation. **Done for WL6/SOD.**
13. Renderer-facing indexed-surface seam. **Initial planar-to-indexed conversion done for WL6/SOD.**
14. Surface metadata/type layer. **Done for WL6/SOD indexed picture surfaces.**
14a. Palette/texture-upload metadata and RGBA expansion. **Done for representative WL6 indexed surface.**
15. SDL-free blit/composite smoke test. **Done for representative WL6 surfaces.**
16. Wall-page metadata decoding and row-major indexed surface seam. **Done for representative WL6/SOD walls.**
17. Raycaster texture-column sampler. **Done for representative WL6/SOD walls.**
18. Fixed-height wall strip scaler. **Done for representative WL6/SOD walls.**
19. Tiny wall-strip viewport smoke test. **Done for representative WL6/SOD walls.**
20. Map-derived wall-hit descriptors feeding viewport seam. **Done for representative WL6 map tiles.**
21. Cardinal map ray stepping helper. **Done for WL6 map 0 player-cardinal rays.**
22. Fixed-point cardinal ray origins/texture offsets. **Done for WL6 player-cardinal rays.**
23. Fixed-point DDA ray helper for arbitrary direction vectors. **Done for WL6 player-origin rays.**
24. Distance/height projected raycast columns. **Done for representative WL6 player-origin rays.**
25. Multi-column projected view batches. **Done for representative WL6 player-origin rays.**
26. Camera/FOV ray-table helper. **Done for representative WL6 player-origin rays.**
27. Tiny view render helper. **Done for representative WL6 player-origin camera rays.**
28. Palette/texture-upload metadata. **Done for representative WL6 indexed surface.**
29. Sprite indexed-surface decoding. **Done for representative WL6 sprites.**
30. Scaled sprite rendering. **Done for representative WL6 sprites.**
31. World-space sprite projection/ordering. **Done for representative WL6 sprite positions.**
32. Combined wall+sprite camera render. **Done for representative WL6 sprite positions.**
33. Runtime actor/static sprite-reference selection. **Done for WL6 map 0 easy difficulty.**
34. VSWAP sprite surface-cache decoding from runtime refs. **Done for representative WL6 refs.**
35. Feed cached ref surfaces into scene renderer. **Done for representative visible WL6 refs.**
36. Palette-effect interpolation metadata. **Done for synthetic 6-bit VGA fade ranges.**
37. Damage/bonus palette shift metadata. **Done for original-style red/white flash targets.**
38. Palette-shift gameplay state. **Done for original-style damage/bonus counters and red-over-white priority.**
39. Palette-shifted upload selection. **Done for base/red/white state-selected upload descriptors.**
40. Broader runtime-ref scene coverage. **Done for five visible WL6 map-0 refs.**
41. Alternate camera runtime-ref scene coverage. **Done for northeast camera vector over the five-ref set.**
42. Officer/SS/mutant runtime actor + scene-ref coverage across additional maps. **Done for selected WL6 maps.**
43. Boss-map runtime actor + scene-ref coverage. **Done for selected WL6 boss map.**
44. Ghost-map runtime actor + scene-ref coverage. **Done for selected WL6 secret map.**
45. Player gameplay event seam for damage/bonus/score/extra-life events. **Done headlessly.**
46. Bonus pickup event seam for ammo/weapons/keys/treasure/health/full-heal/Spear events. **Done headlessly.**
47. Runtime static pickup/removal hook into gameplay events and scene refs. **Done headlessly.**
48. TransformTile-style visible static pickup probe. **Done headlessly.**
49. Player motion/collision tick with pickup probe. **Done headlessly.**
50. Use-button door/elevator/pushwall dispatch seam. **Done headlessly.**
51. Door open/close progression and collision release seam. **Done headlessly.**
52. Pushwall start/progression seam. **Done headlessly.**
53. Runtime solid-plane raycast bridge for live doors/pushwalls. **Done headlessly.**
54. Runtime wall-view render bridge for live tilemap state. **Done headlessly.**
55. Door wall page/texture descriptor seam. **Done headlessly.**
56. Runtime door-aware fixed ray seam. **Done headlessly.**
57. Door-aware runtime camera wall renderer. **Done headlessly.**
58. Door-aware runtime wall+sprite scene renderer. **Done headlessly.**
59. Pushwall wall page/texture descriptor seam. **Done headlessly.**
60. Live pushwall wall+sprite scene occlusion coverage. **Done headlessly.**
61. Pushwall sub-tile distance/height in live runtime rays/rendering. **Done headlessly.**
62. Live runtime-ref scene coverage through door-aware model rendering. **Done headlessly.**
63. Headless live gameplay tick for motion/use/doors/pushwalls/palette. **Done headlessly.**
64. Live tick palette-shifted upload selection. **Done headlessly.**
65. Live tick state driving door-aware scene rendering. **Done headlessly.**
66. Live tick pushwall state driving scene rendering. **Done headlessly.**
67. Live tick static pickup removal driving scene rendering. **Done headlessly.**
68. Actor bite/contact damage event seam. **Done headlessly.**
69. Actor shooting damage event seam. **Done headlessly.**
70. Projectile movement/collision/damage event seam. **Done headlessly.**
71. Live tick projectile damage/palette integration. **Done headlessly.**
72. Live tick actor bite/shoot damage/palette integration. **Done headlessly.**
73. Combined live combat actor/projectile damage and palette integration. **Done headlessly.**
74. Live combat palette-shifted upload selection. **Done headlessly.**
75. Actor hitpoint/damage/kill/drop state. **Done headlessly.**
76. Actor kill drop spawning into runtime statics. **Done headlessly.**
77. Live actor damage/drop spawning tick integration. **Done headlessly.**
78. Live-spawned actor drop scene refs/rendering. **Done headlessly.**
79. Full live combat tick with outgoing actor damage/drop plus incoming actor/projectile damage. **Done headlessly.**
80. Deterministic actor death-state progression. **Done headlessly.**
81. Death-state actor scene refs/rendering. **Done headlessly.**
82. Live full-combat death-state/ref output. **Done headlessly.**
83. Full-combat death-ref scene rendering. **Done headlessly.**
84. Final death-frame replacement in runtime actor refs. **Done headlessly.**
85. Live actor death animation tick/finalization. **Done headlessly.**
86. Full-combat orchestration with active death ticking. **Done headlessly.**
87. Combined full-combat/death-tick final scene rendering. **Done headlessly.**
88. Broaden actor death final-frame refs across enemy classes. **Done headlessly.**
89. Broader WL6 boss/secret map runtime scene-ref coverage. **Done headlessly.**
90. Deeper collision/gameplay events, runtime actor state progression, remaining boss info-tile classification, or SDL3 presentation seam.

## Next Likely Move

Deepen collision/gameplay events, add runtime actor state progression, fill remaining boss info-tile classifications, or add a small SDL3 presentation seam.

Recommended next commit:

- fill remaining unknown boss info-tile classifications for maps such as `Wolf1 Boss` / `Wolf5 Boss`;
- or add a small SDL3 presentation seam using `wl_texture_upload_descriptor`;
- or connect palette-shifted upload selection to future live player damage/bonus events before presentation.

The current harness already verifies WL6 file sizes, `MAPHEAD.WL6` RLEW tag `0xabcd`, map 0 offset/header/name/dimensions, `VSWAP.WL6` header/directory values, bounded chunk-read hashes, representative wall/sprite shape metadata, sprite post-command metadata, sprite indexed-surface hashes, scaled-sprite viewport hashes, world-sprite projection/sorted-render hashes, combined scene render hashes, VGA graphics Huffman chunk hashes, STRUCTPIC dimensions, indexed-surface hashes/descriptors, indexed blit canvas hashes, wall-page metadata/surface hashes, wall texture-column sampler hashes, wall strip scaler/viewport/map-hit/cardinal/fixed/DDA/projected/view-batch/camera-ray/tiny-view canvas hashes, upload metadata/RGBA/palette-fade/shift hashes, shift-state transitions, and palette-selected upload hashes, optional SOD metadata, Carmack/RLEW helper behavior, WL6 map 0 plane hashes/counts, WL6 map 0 semantic classification counts, a WL6 map 0 `SetupGameLevel`-style runtime model, door-area connectivity descriptors, and runtime scene sprite-reference descriptors, VSWAP sprite surface-cache hashes, and broader runtime-scene, camera-scene, multi-map enemy scene-ref, boss-map scene-ref, ghost-map scene-ref, and gameplay-event, bonus-pickup, static-pickup, and visible-static-pickup, player-motion, player-use, door-progression, pushwall-progression, live-solid-ray, live-runtime-render, door-wall-render, runtime-door-ray, runtime-door-render, runtime-door-scene, runtime-pushwall-render, runtime-pushwall-scene, runtime-pushwall-offset, runtime-live-ref-scene, runtime-live-tick, runtime-live-tick-upload, runtime-live-tick-scene, runtime-live-tick-pushwall-scene, and runtime-live-tick-static-scene, and runtime-actor-bite, runtime-actor-shoot, runtime-projectile-damage, runtime-live-projectile-tick, runtime-live-actor-tick, runtime-live-combat-tick, runtime-live-combat-upload, runtime-actor-damage-state, runtime-actor-drop-static, runtime-live-actor-damage-drop, runtime-live-actor-drop-scene, runtime-live-full-combat, runtime-actor-death-state, runtime-actor-death-scene, runtime-live-full-combat-death, runtime-live-full-combat-death-scene, runtime-actor-death-final-frame, runtime-live-actor-death-tick, runtime-live-full-combat-death-tick, runtime-live-full-combat-death-tick-scene, and runtime-actor-death-final-broaden, and runtime-map-boss-scene-broaden assertions.

## Blockers

None. SDL3 integration can safely wait until asset/decompression tests exist.


## Runtime Notes

- Target verification environment is headless Linux.
- Prefer automated CLI/build/unit tests over visual/manual checks.
- Autopilot is expected to continue overnight without asking ET for routine decisions.

## Cycle 2026-04-24 22:22 CDT

Action taken:

- Implemented first runnable modern C + SDL3-port test harness without requiring SDL yet.
- Added `wl_assets` module for local data path resolution, file-size checks, `MAPHEAD` parsing, `GAMEMAPS` map header parsing, and `VSWAP` header parsing.
- Added `source/modern-c-sdl3/Makefile` with strict C11 warnings-as-errors build.
- Added headless test runner `source/modern-c-sdl3/tests/test_assets.c`.
- Added `docs/research/asset-metadata-harness.md` with assertions and verification evidence.
- Updated root `.gitignore` to ignore `source/modern-c-sdl3/build/`.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset metadata tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data.
- Verified `game-files/base/MAPHEAD.WL6` and `game-files/base/VSWAP.WL6` remain ignored by `game-files/.gitignore`.
- Build artifact directory is ignored.

Next likely move:

- Add pure C Carmack + RLEW expansion and decode WL6 map 0 planes into 4096-word arrays with hash/count assertions.

Blockers: none.


## Cycle 2026-04-24 22:26 CDT

Action taken:

- Implemented faithful pure C Carmack expansion and RLEW expansion in `wl_assets`.
- Added `wl_read_map_plane` to read one `GAMEMAPS` plane, Carmack-expand it, validate the embedded RLEW expanded length, and decode to `64x64` words.
- Extended `test_assets` with synthetic decode-helper tests and WL6 map 0 plane 0/1/2 hash/count assertions.
- Updated `source/modern-c-sdl3/README.md`, `.gitignore`, and added `docs/research/map-decompression.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset metadata tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only hashes/counts and metadata are committed.
- Build/cache outputs are ignored.

Next likely move:

- Use decoded planes to classify WL6 map 0 semantics: wall/door/pushwall tiles, static objects, actors, and player start metadata.

Blockers: none.


## Cycle 2026-04-24 22:27 CDT

Action taken:

- Researched original map constants and setup behavior in `WL_DEF.H`, `WL_GAME.C`, `WL_ACT1.C`, and `WL_ACT2.C` without modifying `source/original/`.
- Added `wl_map_semantics` pure C classifier for decoded wall/info planes.
- Extended headless tests with WL6 map 0 semantic assertions: solid/area/door/lock counts, player start, static blockers/bonuses/treasures, pushwalls, path markers, enemy starts by difficulty tier, and unknown-tile check.
- Updated `source/modern-c-sdl3/README.md` and added `docs/research/map-semantics.md`.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only semantic counts/metadata are committed.

Next likely move:

- Build a minimal `SetupGameLevel`-style runtime map model: tilemap, doors, statics, player spawn, and difficulty-filtered actor descriptors.

Blockers: none.


## Cycle 2026-04-24 22:28 CDT

Action taken:

- Added `wl_game_model`, a pure C data-only runtime model for decoded WL6 map 0 planes.
- Modeled original-style tilemap initialization, door descriptors/side markers, static descriptors, player spawn, pushwalls/secrets, path markers, and difficulty-filtered guard/dog/dead-guard actor descriptors.
- Extended tests with easy/medium/hard descriptor and total assertions.
- Updated `source/modern-c-sdl3/README.md` and added `docs/research/runtime-map-model.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only model counts/descriptors are committed.

Next likely move:

- Add door-area connectivity descriptors from wall-plane area tiles, or pivot to full VSWAP chunk descriptors if connectivity needs more original-source research.

Blockers: none.


## Cycle 2026-04-24 22:38 CDT

Action taken:

- Extended `wl_game_model` door descriptors with `DoorOpening`/`DoorClosing` area connectivity pairs.
- Added symmetric door-area connection counts to model the original `areaconnect` increments when doors open.
- Added WL6 map 0 assertions for first/lock-5 door area pairs, total door links, unique area-pairs, and representative matrix counts.
- Updated `docs/research/runtime-map-model.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only connectivity metadata/counts are committed.

Next likely move:

- Implement full VSWAP chunk descriptors: all offsets/lengths, range validation, sparse chunk handling, and first wall/sprite/sound metadata assertions.

Blockers: none.


## Cycle 2026-04-24 22:41 CDT

Action taken:

- Implemented full VSWAP chunk-directory parsing in `wl_assets`.
- Added chunk kind classification for wall/sprite/sound/sparse ranges.
- Validated table bounds, file bounds, monotonic non-overlap, max chunk end, and aggregate range counts.
- Added WL6 assertions and optional SOD assertions for first/last wall, first sprite, first sound, final chunk, counts, and file end.
- Updated `source/modern-c-sdl3/README.md` and added `docs/research/vswap-directory.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only VSWAP metadata/count assertions are committed.

Next likely move:

- Add bounded VSWAP chunk read smoke tests with stable hashes, then start wall/sprite shape metadata decoding.

Blockers: none.


## Cycle 2026-04-24 22:44 CDT

Action taken:

- Added `wl_read_vswap_chunk`, a bounded chunk-read helper using validated VSWAP directory metadata.
- Added WL6 read smoke assertions for representative wall, sprite, sound, and final chunks using length + FNV-1a hashes only.
- Added matching optional SOD read smoke assertions.
- Updated `source/modern-c-sdl3/README.md` and `docs/research/vswap-directory.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap-read tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only chunk lengths/hashes are committed.

Next likely move:

- Begin VSWAP wall/sprite shape metadata decoding from bounded chunk reads.

Blockers: none.


## Cycle 2026-04-24 22:47 CDT

Action taken:

- Added `wl_decode_vswap_shape_metadata` for safe metadata-only VSWAP wall/sprite chunks.
- Wall chunks now assert canonical `64x64` raw page metadata.
- Sprite chunks now validate `t_compshape`-style left/right pixel bounds, visible-column count, and packed column-offset table range/monotonicity.
- Added WL6 and optional SOD assertions for representative wall and first sprite metadata.
- Updated `docs/research/vswap-directory.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap-shape tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash assertions are committed.

Next likely move:

- Parse sprite post-command streams more deeply, or start VGAHEAD/VGAGRAPH/VGADICT Huffman smoke tests.

Blockers: none.


## Cycle 2026-04-24 22:50 CDT

Action taken:

- Extended `wl_decode_vswap_shape_metadata` to walk representative sprite post-command lists.
- Validates each post's start/end pixel*2 bounds, column zero terminators, posts-per-column bounds, source-offset ranges, and aggregate post spans.
- Added WL6 assertions for sprite chunks `106` and `107`; added matching optional SOD assertions for chunks `134` and `135`.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap-sprite-post tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash/count assertions are committed.

Next likely move:

- Start VGAHEAD/VGAGRAPH/VGADICT Huffman smoke tests, or decode wall-page metadata from raw VSWAP wall chunks.

Blockers: none.


## Cycle 2026-04-24 22:53 CDT

Action taken:

- Added `VGAHEAD` 3-byte graphics-offset parsing with sparse sentinel support.
- Added `VGADICT` 255-node Huffman dictionary parsing.
- Ported the original LSB-first `CAL_HuffExpand` traversal into pure C as `wl_huff_expand`.
- Added explicit-size `VGAGRAPH` chunk read/decode helper.
- Added WL6 smoke assertions for chunks `0`, `1`, `87`, and `134`; added optional SOD assertions for chunks `0`, `1`, `3`, and `149`.
- Added `docs/research/graphics-huffman.md` and updated `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/vga-huffman tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash/count assertions are committed.

Next likely move:

- Interpret decoded `STRUCTPIC` picture metadata, add a renderer-facing indexed-surface seam, or decode VSWAP wall-page metadata.

Blockers: none.


## Cycle 2026-04-24 22:57 CDT

Action taken:

- Added `wl_decode_picture_table` for decoded `STRUCTPIC` chunks.
- Interprets original `pictabletype` entries as 16-bit width/height pairs.
- Added WL6 assertions for picture count, dimension ranges, total declared pixels, and representative dimensions.
- Added matching optional SOD picture-table assertions.
- Updated `docs/research/graphics-huffman.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/vga-pictable tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash/count/dimension assertions are committed.

Next likely move:

- Start a renderer-facing indexed-surface seam for decoded pictures, or decode VSWAP wall-page metadata.

Blockers: none.


## Cycle 2026-04-24 22:59 CDT

Action taken:

- Added `wl_decode_planar_picture_to_indexed`.
- Converts original VGA planar picture chunks into linear 8-bit indexed surfaces suitable for a future SDL3 texture/upload boundary.
- Added WL6 indexed-surface hash assertions for representative picture chunks `3`, `87`, and `134`.
- Added optional SOD indexed-surface hash assertions for chunks `3`, `90`, and `149`.
- Updated `docs/research/graphics-huffman.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/vga-surface tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash/count/dimension assertions are committed.

Next likely move:

- Add a small surface metadata/type layer for decoded indexed pictures, or decode VSWAP wall-page metadata.

Blockers: none.


## Cycle 2026-04-24 23:04 CDT

Action taken:

- Added `wl_surface_format`, `wl_indexed_surface`, and `wl_wrap_indexed_surface`.
- Added `wl_decode_planar_picture_surface` to convert planar VGA picture chunks and wrap the result with renderer-facing metadata.
- Tests now assert indexed-8 format, width, height, stride, pixel count, pixel pointer, and stable surface hashes for representative WL6 and optional SOD pictures.
- Updated `docs/research/graphics-huffman.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/vga-surface-layer tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash/count/dimension assertions are committed.

Next likely move:

- Decode VSWAP wall-page metadata, or add an SDL-free blit/composite smoke test consuming `wl_indexed_surface`.

Blockers: none.


## Cycle 2026-04-24 23:07 CDT

Action taken:

- Added `wl_blit_indexed_surface`, an SDL-free clipped blitter for `wl_indexed_surface`.
- Added WL6 canvas hash assertions after compositing representative decoded picture surfaces.
- Updated `docs/research/graphics-huffman.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/vga-blit tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only decoded metadata/hash/count/dimension assertions are committed.

Next likely move:

- Decode VSWAP wall-page metadata, or add palette/texture-upload metadata for future SDL3 texture integration.

Blockers: none.


## Cycle 2026-04-24 23:10 CDT

Action taken:

- Added `wl_wall_page_metadata`, `wl_decode_wall_page_metadata`, `wl_decode_wall_page_to_indexed`, and `wl_decode_wall_page_surface`.
- Converted raw VSWAP wall pages from original column-major `PM_GetPage(wallpic) + texture` layout into caller-owned row-major indexed surfaces for future raycaster/SDL3 seams.
- Added representative WL6 and optional SOD wall-page metadata assertions: color ranges, unique color counts, raw hashes, and row-major surface hashes.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/wall-page tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Build a raycaster texture-column sampler or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:14 CDT

Action taken:

- Added `wl_sample_wall_page_column`, sampling a canonical 64-byte wall texture column from the original column-major `PM_GetPage(wallpic) + texture` layout.
- Added `wl_sample_indexed_surface_column`, sampling the same column from row-major indexed surfaces and cross-checking equivalence.
- Added representative WL6 and optional SOD column hash assertions plus invalid-offset/bounds checks.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/wall-column tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Build a fixed-height/fixed-step wall strip scaler, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:16 CDT

Action taken:

- Added `wl_scale_wall_column_to_surface`, a pure C fixed-height wall strip scaler consuming sampled 64-byte wall texture columns.
- Mirrors the original `BuildCompScale` source-pixel run distribution in a headless linear indexed-surface form, clipping to the destination height.
- Added representative WL6 and optional SOD scaled-strip canvas hash assertions plus invalid input checks.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/wall-scaler tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Use the wall strip scaler in a tiny deterministic viewport/raycast smoke test, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:19 CDT

Action taken:

- Added `wl_wall_strip` and `wl_render_wall_strip_viewport`, an SDL-free wall-strip viewport composition seam.
- The viewport renderer consumes ordered wall strip commands, samples VSWAP wall texture columns, and scales them into caller-owned indexed surfaces.
- Added representative WL6 and optional SOD viewport hash assertions plus empty-command validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/wall-viewport tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Feed the viewport seam actual map/ray hit descriptors, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:22 CDT

Action taken:

- Added `wl_wall_side`, `wl_map_wall_hit`, `wl_build_map_wall_hit`, and `wl_wall_hit_to_strip`.
- Converts decoded map-plane wall tiles into original-style horizontal/vertical wall page indices, texture offsets, and viewport strip descriptors.
- Added WL6 map-derived viewport assertions using representative wall tiles from map 0, including non-wall rejection.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/map-hit tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add a first DDA/raycast stepping helper that emits `wl_map_wall_hit` descriptors, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:25 CDT

Action taken:

- Added `wl_cardinal_ray_direction` and `wl_cast_cardinal_wall_ray`.
- The helper walks decoded map-plane tiles north/east/south/west from a starting tile, finds the first solid wall tile, and emits a `wl_map_wall_hit` descriptor.
- Added WL6 map 0 player-cardinal ray assertions and rendered their resulting viewport strips through the existing SDL-free path.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/cardinal-ray tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Generalize toward fixed-point DDA/raycast columns, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:28 CDT

Action taken:

- Added `wl_cast_fixed_cardinal_wall_ray`, accepting 16.16 world origins and cardinal direction.
- The helper derives original-style texture columns from the perpendicular fixed-point intercept coordinate and emits `wl_map_wall_hit` descriptors through the existing viewport path.
- Added WL6 player-center fixed-ray assertions matching the cardinal ray viewport hash, plus out-of-map validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/fixed-ray tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Generalize fixed cardinal stepping toward full DDA/raycast columns, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:30 CDT

Action taken:

- Added `wl_cast_fixed_wall_ray`, a pure C 16.16 fixed-point DDA helper for arbitrary direction vectors.
- The helper walks vertical/horizontal map-grid crossings, derives texture columns from the fixed-point wall intercept, and emits `wl_map_wall_hit` descriptors into the existing SDL-free viewport path.
- Added WL6 player-origin DDA assertions covering cardinal equivalence, a northeast off-axis ray, deterministic viewport hash `0xae40b70c`, zero-vector validation, and out-of-map validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/dda-ray tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add distance/height projection and screen-column batch descriptors on top of the fixed DDA helper, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:34 CDT

Action taken:

- Added `wl_project_wall_height`, using the original `FOCALLENGTH`, `MINDIST`, `VIEWGLOBAL`, and `heightnumerator/(distance>>8)` projection shape to convert fixed DDA distances into deterministic strip heights.
- Added `wl_cast_projected_wall_ray`, which wraps the 16.16 DDA ray helper and emits `wl_map_wall_hit` descriptors with projected `scaled_height` values for the existing SDL-free viewport path.
- Extended WL6 player-origin assertions with projected distance/height checks, deterministic projected viewport hash `0xd48f2f6d`, and invalid screen-column validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/projected-ray tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add a multi-column projected view batch helper, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:38 CDT

Action taken:

- Added `wl_cast_projected_wall_ray_batch`, a pure C helper that validates and emits ordered screen-column `wl_map_wall_hit` descriptors from caller-provided 16.16 ray vectors.
- Extended WL6 player-origin assertions with a five-column projected batch, deterministic viewport hash `0x7209a9ed`, and invalid batch bounds/count validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/view-batch tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add a small camera/FOV ray-table helper feeding projected batches, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:41 CDT

Action taken:

- Added `wl_build_camera_ray_directions`, a pure C helper that generates half-pixel-centered 16.16 ray vectors from a forward vector and camera-plane vector.
- Fed those generated rays into `wl_cast_projected_wall_ray_batch` for representative WL6 player-origin columns.
- Added deterministic assertions for generated ray directions, wall-hit descriptors, viewport hash `0x7320f695`, and invalid camera table bounds/zero-forward validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/camera-rays tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add a tiny view render helper that builds camera rays, casts projected batches, maps VSWAP wall pages, and renders strips, or add palette/texture-upload metadata before SDL3 presentation.

Blockers: none.


## Cycle 2026-04-24 23:44 CDT

Action taken:

- Added `wl_render_camera_wall_view`, a pure C helper that builds camera rays, casts projected DDA batches, maps wall-page indices to caller-supplied VSWAP wall pages, and renders wall strips into a caller-owned indexed viewport.
- Extended WL6 player-origin assertions with deterministic tiny-view hit descriptors, viewport hash `0xfad71929`, and missing wall-page validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/view-render tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add palette/texture-upload metadata that can connect indexed surfaces and wall views to SDL3 textures, or add a minimal SDL3 presentation seam after that metadata is covered.

Blockers: none.


## Cycle 2026-04-24 23:47 CDT

Action taken:

- Added `wl_texture_upload_descriptor` plus indexed-8/RGB-palette and RGBA8888 upload formats.
- Added `wl_describe_indexed_texture_upload` to describe indexed surfaces with 256-entry RGB palettes for future SDL3 upload.
- Added `wl_expand_indexed_surface_to_rgba` to expand indexed surfaces through 6-bit VGA DAC or 8-bit palette components into RGBA8888 without requiring SDL.
- Extended WL6 picture-surface assertions with upload descriptor checks, deterministic synthetic-palette RGBA hash `0xb75bdee9`, and invalid palette/size validation.
- Updated `docs/research/graphics-huffman.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/upload-metadata tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add a small SDL3 presentation seam using the upload descriptors, or expand renderer metadata for sprites/palette effects before presentation.

Blockers: none.


## Cycle 2026-04-24 23:50 CDT

Action taken:

- Added `wl_decode_sprite_shape_to_indexed` and `wl_decode_sprite_shape_surface`, decoding VSWAP sprite post-command streams into caller-owned transparent `64x64` indexed surfaces.
- Extended WL6 sprite assertions for chunks `106` and `107` with surface descriptors, non-transparent pixel counts, deterministic hashes `0x918ed728` and `0x88a2d1b4`, and undersized-output validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.
- SDL3 was not available via `pkg-config` in this environment, so this cycle advanced renderer metadata rather than pretending to verify SDL presentation.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/sprite-surface tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add scaled sprite rendering using decoded sprite surfaces and existing wall-height/viewport metadata, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-24 23:54 CDT

Action taken:

- Added `wl_render_scaled_sprite`, a pure C transparent sprite compositor for caller-owned indexed viewports.
- The scaler uses source-run distribution, centered screen placement, destination clipping, transparent-index skipping, and optional per-column wall-height occlusion (`wall_heights[x] >= scaled_height` skips the sprite column), matching the original `ScaleShape` visibility seam without DOS/VGA dependencies.
- Extended WL6 sprite chunk `106` assertions with normal, wall-occluded, and left-clipped scaled viewport hashes: `0x3f753ac8`, `0xaa7c2838`, and `0x6ff0f5c8`, plus invalid-argument checks.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.
- SDL3 remains unavailable via `pkg-config`, so this cycle continued deterministic renderer work that can feed SDL later.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/scaled-sprite tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add world-space sprite projection/ordering that feeds decoded/scaled sprite surfaces into the current camera wall-view seam, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-24 23:57 CDT

Action taken:

- Added `wl_projected_sprite`, `wl_project_world_sprite`, and `wl_sort_projected_sprites_far_to_near`.
- The projection seam mirrors the original `TransformActor`/`TransformTile` math shape: focal viewpoint offset, fixed-point forward/lateral transform, `ACTORSIZE` forward-distance fudge, `mindist` visibility rejection, screen-x projection, and original-style projected height.
- Extended WL6 assertions with representative sprite centers in front of the map-0 player, far-to-near sorting, an invisible/behind-player sprite, and a sorted render hash `0x819b1035` through the existing transparent scaled-sprite compositor.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.
- SDL3 remains unavailable via `pkg-config`, so this cycle advanced the deterministic headless renderer seam.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/world-sprite tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Add a combined wall+sprite camera render helper that reuses projected sprite descriptors, decoded sprite surfaces, and wall-height occlusion metadata, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:00 CDT

Action taken:

- Added `wl_render_camera_scene_view`, a pure C combined wall+sprite indexed scene renderer.
- The helper renders camera wall columns, derives a per-column wall-height occlusion buffer from the wall hits, projects world-space sprite centers, sorts sprites far-to-near, and composites transparent indexed sprite surfaces into the same viewport.
- Extended WL6 assertions with two projected representative sprites, far/near surface-index preservation, wall-height occlusion checks, combined scene hash `0x1e4a8264`, and invalid sprite-surface validation.
- Updated `docs/research/vswap-directory.md` and `source/modern-c-sdl3/README.md`.
- SDL3 remains unavailable via `pkg-config`, so this cycle advanced the deterministic headless renderer seam.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/scene-render tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Connect runtime actor/static descriptors to sprite chunk selection for the combined scene renderer, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:03 CDT

Action taken:

- Added `wl_scene_sprite_ref` and `wl_collect_scene_sprite_refs`, bridging the runtime map model to renderer sprite inputs.
- Static refs map original `statinfo`-style types to `SPR_STAT_*` shapenum values; actor refs map initial guard/dog/dead-guard states to starting shapenum values.
- Each ref carries kind, model index, shapenum source index, VSWAP chunk index (`PMSpriteStart + shapenum`), and 16.16 world-center coordinates.
- Extended WL6 easy map assertions with `133` refs (`121` statics + `12` actors), representative static/guard/dog/dead-guard chunk indices, descriptor hash `0x2ab36473`, and small-buffer failure validation.
- Updated `docs/research/runtime-map-model.md` and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/sprite-ref tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Use scene sprite refs to decode/cache needed VSWAP sprite surfaces and feed them into the combined wall+sprite scene renderer, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:06 CDT

Action taken:

- Added `wl_decode_vswap_sprite_surface_cache`, decoding caller-provided VSWAP sprite chunk-index lists into contiguous caller-owned `64x64` indexed surfaces.
- Corrected sprite post decoding to honor original 16-bit wraparound of corrected top/source offsets, which is required by some static sprite chunks such as WL6 chunk `134`.
- Extended WL6 assertions by feeding runtime scene sprite refs into the cache decoder for representative static/guard/dog/dead-guard chunks, with per-surface hashes `0x38769770`, `0xbd6176ba`, `0x0fe580fa`, `0xa875d685`, `0x63f7eba2`, combined cache hash `0x4a8eb8db`, and invalid wall/small-buffer checks.
- Updated `docs/research/vswap-directory.md`, `docs/research/runtime-map-model.md`, and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/sprite-cache tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Feed runtime-ref decoded sprite surfaces directly into `wl_render_camera_scene_view`, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:10 CDT

Action taken:

- Added a headless runtime-ref scene-render smoke path in `tests/test_assets.c`.
- Selected visible WL6 map-0 runtime scene refs `113` and `114`, decoded their VSWAP sprite chunks through `wl_decode_vswap_sprite_surface_cache`, and passed the resulting cached surfaces plus ref world coordinates/source ids into `wl_render_camera_scene_view`.
- Asserted cache hashes `0x442facd4`/`0xd363bf0c`, combined cache hash `0xd53b06f5`, sorted projected draw order (`source_index` `26` then `16`), screen-x/height descriptors, and final indexed scene hash `0x61f7f78b`.
- Updated `docs/research/vswap-directory.md`, `docs/research/runtime-map-model.md`, and `source/modern-c-sdl3/README.md`.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/ref-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Broaden runtime-ref scene coverage, add palette/fade-effect metadata, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:14 CDT

Action taken:

- Added `wl_interpolate_palette_range`, a pure C helper matching the original `VL_FadeOut`/`VL_FadeIn` integer palette interpolation shape over inclusive palette-index ranges.
- Extended WL6 headless tests with synthetic 6-bit VGA palette fade assertions: full-range fade hash `0xa93a5ba5`, range-final hash `0x91f102c5`, faded RGBA sample hash `0x50918d48`, and invalid range/component/step validation.
- Updated `docs/research/graphics-huffman.md`, `source/modern-c-sdl3/README.md`, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/palette-fade tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Broaden palette-effect coverage toward game red/white flashes, broaden runtime-ref scene coverage, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:16 CDT

Action taken:

- Added `wl_build_palette_shift`, a pure C helper for original-style full-palette gameplay flashes.
- Modeled the `WL_PLAY.C::InitRedShifts` targets: red damage shifts toward `(64,0,0)` with `REDSTEPS=8`, and white/bonus shifts toward `(64,62,0)` with `WHITESTEPS=20`.
- Extended WL6 headless tests with red-shift hash `0xb8462fc5`, red-shift RGBA sample hash `0xfa0a0cd7`, white-shift hash `0x3c8da1ed`, white-shift RGBA sample hash `0x93adda7f`, endpoint component assertions, and invalid target/component/step validation.
- Updated `docs/research/graphics-huffman.md`, `source/modern-c-sdl3/README.md`, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/palette-shift tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash assertions are committed.

Next likely move:

- Connect palette-shift state to damage/bonus counters, broaden runtime-ref scene coverage, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:19 CDT

Action taken:

- Added `wl_palette_shift_state`, `wl_palette_shift_result`, and reset/start/update helpers for the original palette flash counter behavior.
- Mirrored `WL_PLAY.C::StartBonusFlash`, `StartDamageFlash`, and `UpdatePaletteShifts`: bonus starts at `NUMWHITESHIFTS * WHITETICS`, damage accumulates, update selects white/red before decrementing counters, clamps counts to zero, prioritizes red over white, and emits a base-palette reset after shifted frames clear.
- Extended headless tests with no-op, bonus-only, overlapping damage+bonus, base-reset, and invalid-input state transitions.
- Updated `docs/research/graphics-huffman.md`, `source/modern-c-sdl3/README.md`, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/palette-state tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Connect this palette state to future live gameplay/player events, broaden runtime-ref scene coverage, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:21 CDT

Action taken:

- Added `wl_select_palette_for_shift` and `wl_describe_palette_shifted_texture_upload`, connecting palette-shift state results to renderer-facing indexed texture upload metadata.
- Headless tests now precompute original-style red/white shift tables, step through bonus-only, red-over-white, and base-reset states, select the effective palette, describe the shifted texture upload, and expand a small indexed surface to RGBA.
- Added assertions for selected palette pointers/hashes, state-selected white RGBA hash `0x93adda7f`, state-selected red-step-3 hash `0x90a6cdc5` / RGBA hash `0xd742b64a`, base reset RGBA hash `0xccd1a665`, and invalid out-of-range shift selection.
- Confirmed SDL3 dev files are still unavailable via `pkg-config`, so this remains a verified SDL-free presentation boundary.
- Updated `docs/research/graphics-huffman.md`, `source/modern-c-sdl3/README.md`, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/palette-upload tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect this palette upload seam to future live gameplay/player events, broaden runtime-ref scene coverage, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:23 CDT

Action taken:

- Broadened the headless runtime-ref scene smoke test from two visible refs to five WL6 map-0 refs: `110`, `111`, `113`, `114`, and `115`.
- Decoded those runtime ref chunks through `wl_decode_vswap_sprite_surface_cache`, including duplicate chunk reuse, and asserted per-surface hashes plus combined cache hash `0x61a879ca`.
- Fed the cached surfaces plus runtime world coordinates/source ids into `wl_render_camera_scene_view`; asserted sorted projected source order `21,11,26,16,16`, representative surface indices/view descriptors, and final indexed scene hash `0xb92e568b`.
- Updated `docs/research/vswap-directory.md`, `docs/research/runtime-map-model.md`, `source/modern-c-sdl3/README.md`, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/broad-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add more map/camera runtime-scene coverage, connect renderer/palette seams to future live gameplay events, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:26 CDT

Action taken:

- Added an alternate-camera runtime-ref scene assertion over the existing five visible WL6 map-0 refs.
- Reused the cached runtime sprite surfaces and rendered the scene with a northeast camera vector (`forward=(0x10000,-0x4000)`, `plane=(0,-0x8000)`).
- Asserted shifted sorted sprite descriptors, including view-x/height/distance changes, and final indexed scene hash `0x4668f191`.
- Updated `docs/research/vswap-directory.md`, `docs/research/runtime-map-model.md`, `source/modern-c-sdl3/README.md`, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/camera-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add additional map runtime-scene coverage, connect renderer/palette seams to future live gameplay events, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:38 CDT

Action taken:

- Extended `wl_build_game_model` from guard/dog/dead-guard spawning to original-style officer, SS, and mutant stand/patrol tile ranges with difficulty gates.
- Extended `wl_collect_scene_sprite_refs` to map officer/SS/mutant initial states to renderer-facing VSWAP sprite indices.
- Added headless WL6 model coverage for `Wolf1 Map2`, `Wolf2 Map1`, and `Wolf3 Map1`, asserting counts, zero unknown info tiles, actor-kind totals, scene-ref counts, and scene-ref descriptor hashes (`0xab87ed41`, `0x89b8f3c0`, `0xc090c2df`).
- Updated runtime/VSWAP docs, README, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/enemy-map-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add boss/ghost runtime coverage, connect renderer/palette seams to live gameplay events, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:43 CDT

Action taken:

- Added data-only WL6 boss tile handling to `wl_build_game_model` for the non-Spear boss spawns, preserving original starting north/south directions and kill-total/shootable behavior.
- Extended `wl_collect_scene_sprite_refs` to map known boss source tiles to their original starting VSWAP sprite indices.
- Added headless `Wolf2 Boss` coverage asserting model totals, two mutants plus one boss actor, zero unknown info tiles, scene-ref count `168`, and scene-ref descriptor hash `0xb2dab28b`.
- Updated runtime/VSWAP docs, README, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/boss-map-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add ghost runtime coverage, connect renderer/palette seams to live gameplay events, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:48 CDT

Action taken:

- Added data-only WL6 ghost tile handling to `wl_build_game_model` for `SpawnGhosts` (`en_blinky`, `en_clyde`, `en_pinky`, `en_inky`).
- Preserved original ghost traits: east starting direction, non-shootable flag, ambush-style ghost mode, and kill-total increment.
- Extended `wl_collect_scene_sprite_refs` to map ghost source tiles `224..227` to their starting VSWAP sprite indices.
- Added headless `Wolf3 Secret` coverage asserting four ghosts, zero unknown info tiles, scene-ref count `134`, and scene-ref descriptor hash `0xe03fdb45`.
- Updated runtime/VSWAP docs, README, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/ghost-map-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect renderer/palette seams to live gameplay events, add broader map runtime-scene coverage, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:54 CDT

Action taken:

- Added `wl_gameplay`, a pure C player event seam for original-style damage, healing, bonus flash, scoring, and extra-life thresholds.
- Connected damage/bonus events to the existing palette-shift state so future live gameplay can drive red/white flash upload selection.
- Mirrored key original behaviors: victory damage no-op, baby-difficulty quarter damage, god-mode health preservation with flash, death state, `gotgatgun` reset, 100-health healing clamp, `EXTRAPOINTS=40000`, and lives capped at 9 while thresholds keep advancing.
- Updated build wiring, tests, README, graphics/runtime docs, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/gameplay-events tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden map runtime-scene coverage, add more gameplay event semantics, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 00:58 CDT

Action taken:

- Extended `wl_gameplay` with original-style pickup helpers for ammo, weapons, keys, and `GetBonus`-style bonus items.
- Covered ammo restoration from knife, 99-ammo clamp, ammo-full rejection, key bitmasks, machinegun/chaingun promotion, chaingun `gotgatgun`, treasure point awards, health pickup gates, full-heal one-up/ammo/treasure behavior, Spear completion state, and bonus flash only on picked-up items.
- Added headless assertions to the gameplay event test and updated README/docs/state.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/bonus-events tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect bonus pickup semantics to runtime static-object collision/removal, broaden map runtime-scene coverage, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:03 CDT

Action taken:

- Added an `active` flag to runtime static descriptors and taught scene-ref collection to skip inactive statics.
- Added `wl_try_pickup_static_bonus`, mapping runtime static types to gameplay bonus items and clearing the static only after a successful pickup.
- Headless tests now assert full-health food rejection leaves the static active, successful food pickup heals, starts bonus flash, deactivates the static, and drops renderer-facing scene refs from `133` to `132`.
- Updated README, graphics/runtime docs, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/static-pickup tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden map runtime-scene coverage, deepen collision/gameplay events, or add a small SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:08 CDT

Action taken:

- Added `wl_try_pickup_visible_static_bonus`, a headless bridge from renderer visibility math to gameplay pickups.
- The probe mirrors the original `TransformTile` bonus pickup window: active bonus statics are eligible only when they are in front of the player, close enough, and within half-tile lateral range.
- Tests now cover a lateral miss, a full-health in-range food rejection that leaves the static active, and a successful in-range food pickup that heals, starts bonus flash, deactivates the static, and drops scene refs from `133` to `132`.
- Updated README, runtime research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/visible-static-pickup tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add a small player movement/collision step that calls the visible pickup probe during a headless tick, broaden map runtime-scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:12 CDT

Action taken:

- Added `wl_player_motion_state`, `wl_init_player_motion_from_spawn`, and `wl_step_player_motion` as a small headless player movement/collision seam.
- The movement step mirrors the original `ClipMove` shape: try combined movement, then X-only, then Y-only, otherwise remain blocked.
- Collision checks use `PLAYERSIZE`/`MINDIST`-style bounds against solid tiles, door centers, and active blocking statics, then call the visible static pickup probe after movement.
- Tests now assert WL6 spawn centering, a moving food-pickup tick that heals/deactivates the static, and a blocking-static collision that prevents movement.
- Updated README, runtime research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/player-motion tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add door/elevator/use-button semantics, broaden map runtime-scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:16 CDT

Action taken:

- Added `wl_use_player_facing`, a headless `Cmd_Use`-style dispatch seam for facing-tile interactions.
- Door descriptors now carry original-style action state (`open`, `closed`, `opening`, `closing`) so use events can toggle doors deterministically.
- The use seam detects pushwall markers, flips east/west elevator switches and marks completion/secret completion, rejects locked doors without keys, opens keyed closed doors, and closes already-opening doors.
- Tests cover pushwall metadata, secret elevator completion/switch flip, locked-door rejection, keyed opening, and closing an opening door.
- Updated README, runtime research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/player-use tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add door open/close progression/collision release, pushwall movement metadata, broaden map runtime-scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:20 CDT

Action taken:

- Added `wl_step_doors`, a headless `MoveDoors`-style progression seam for opening/open/closing/closed runtime doors.
- Runtime door descriptors now carry original-style `doorposition` and `ticcount` equivalents.
- Opening advances by `tics << 10`, full-open doors release collision by clearing the door-center tile, open doors wait for `OPENTICS=300`, closing restores the solid marker, and blocked closing reopens.
- Tests cover partial/full opening, collision release, open wait, close start/collision restore, blocked close reopen, and full close.
- Updated README, runtime research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/door-progression tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add pushwall movement metadata, broaden map runtime-scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:24 CDT

Action taken:

- Added runtime pushwall motion state plus `wl_start_pushwall` and `wl_step_pushwall`, mirroring original `PushWall`/`MovePWalls` metadata transitions.
- Pushwall use now starts a push when possible: it requires a solid source tile and clear destination, marks the source with `0xc0`, reserves the next tile, and increments the player secret count.
- Progression advances `pwallstate` by tics, reports `pwallpos=(state/2)&63`, clears vacated tiles at 128-tic boundaries, shifts the moving marker forward, reserves the second tile, and stops after the second block crossing.
- Tests cover start metadata, secret count, sub-tile position, first crossing, reserved destination, and final stop.
- Updated README, runtime research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/pushwall-progression tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden runtime-scene coverage with updated live-state removal/movement seams, add actor/player interaction events, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:28 CDT

Action taken:

- Added `wl_build_runtime_solid_plane`, bridging mutable runtime `tilemap` state into the existing wall-ray helpers.
- The bridge treats clear floor/door-side markers as empty, preserves normal wall ids, maps closed door centers to a caller-selected solid placeholder until textured door rendering exists, and preserves moving pushwall wall ids from the low six bits of the `0xc0` marker.
- Tests prove live state changes affect ray hits: closed door blocks an east ray, clearing it lets the ray hit a later wall, and a moving pushwall marker becomes the first ray hit with its original wall id.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/live-solid-ray tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add textured door/pushwall render specialization, broaden runtime-scene coverage with live-state seams, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:14 CDT

Action taken:

- Added `wl_render_runtime_camera_wall_view`, a headless wrapper that converts mutable `wl_game_model::tilemap` state through `wl_build_runtime_solid_plane` and feeds the existing camera wall-view renderer.
- Added deterministic render coverage for live door state: a closed door center blocks/renders as a placeholder solid, then clearing that door makes the same rays continue to a later wall.
- The closed-door runtime canvas hash is `0x62d02b0d`; the open-door continuation hash is `0x32a9148e`.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/live-runtime-render tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add textured/animated door or pushwall render specialization, broaden live-state scene coverage, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:19 CDT

Action taken:

- Added `wl_build_door_wall_hit`, a renderer-facing descriptor seam for original-style door wall posts.
- The helper derives `DOORWALL = PMSpriteStart - 8`, selects normal/locked/elevator door page groups from runtime door lock metadata, applies the vertical-door page offset, and subtracts runtime `doorposition` from the fixed-point intercept for texture-column selection.
- Added headless assertions for normal and locked vertical door descriptors, including an opening-position texture shift, and rendered a locked vertical door strip with canvas hash `0x40d8b9a5`.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/door-wall-render tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Integrate door descriptors into projected runtime ray hits, add animated pushwall render descriptors, broaden live-state scene coverage, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:23 CDT

Action taken:

- Added `wl_cast_runtime_fixed_wall_ray`, a live-model fixed DDA ray seam that reads mutable `wl_game_model::tilemap` state directly.
- Door-center markers now resolve through runtime door descriptors and emit door page/texture metadata with `doorposition` applied; door-side markers are skipped and moving pushwall markers preserve their original wall id.
- Tests assert a locked opening door hit (`page 105`, texture `0x0400`, distance `0x8000`), clearing that door lets the ray continue to a later wall (`page 3`, distance `0x28000`), and a moving pushwall marker emits wall page `73`.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-door-ray tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Integrate runtime door-aware rays into the camera wall/scene renderer, add pushwall render descriptors, broaden live-state scene coverage, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:41 CDT

Action taken:

- Added `wl_render_runtime_door_camera_scene_view`, a combined wall+sprite renderer that uses live runtime door-aware wall hits.
- The helper fills the sprite occlusion buffer from live door/wall hits, then projects/sorts/composites transparent sprite surfaces like the existing immutable-map scene renderer.
- Tests place a sprite behind a locked opening door and assert distinct closed/open scene hashes: `0x01053e89` when the door blocks/occludes and `0xa06c2183` after the door is cleared.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-door-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add pushwall-specific render descriptors, broaden live-state scene coverage with real runtime refs and door/pushwall state, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:44 CDT

Action taken:

- Added `wl_build_pushwall_wall_hit`, a renderer-facing descriptor seam for original-style moving pushwalls.
- The helper preserves the source wall id from the low six bits of the `0xc0` moving marker, selects horizontal/vertical wall page indices, and mirrors `HitHorizPWall` / `HitVertPWall` texture-column flipping by ray step direction.
- `wl_cast_runtime_fixed_wall_ray` now routes moving pushwall tilemap entries through the pushwall descriptor seam.
- Tests assert horizontal/vertical pushwall page+texture descriptors, invalid inputs, and the live runtime ray path still emits the expected moving-wall page/texture metadata.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden live-state scene coverage with real runtime refs and door/pushwall state, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:50 CDT

Action taken:

- Broadened live runtime scene coverage from door occlusion to moving pushwall occlusion.
- The test now supplies the real local VSWAP wall page for a `37 | 0xc0` moving pushwall marker, renders a sprite behind it through `wl_render_runtime_door_camera_scene_view`, clears the moving marker, and verifies the open scene differs.
- Asserted pushwall hit metadata (`tile_x=5`, wall page `73`) and deterministic scene hashes: blocked `0x81e9da6b`, open `0xf80cfa3f`.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Account for pushwall sub-tile `pwallpos` distance/height in runtime rendering, broaden live scenes with real runtime refs/statics, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 01:55 CDT

Action taken:

- Connected moving pushwall `pwallpos` to live runtime ray distance and projected wall height.
- The runtime pushwall ray path now mirrors the original `HitHorizPWall` / `HitVertPWall` shape: texture selection comes from the grid intercept, while `pwallpos << 10` offsets the hit depth before projection.
- Tests assert a direct pushwall ray shifts from distance `0x8000` to `0x0c000` at `pwallpos=16`, and the live wall+sprite scene changes from base hash `0x81e9da6b` to shifted hash `0x83a0d93b` before clearing to `0xf80cfa3f`.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden live scenes with real runtime refs/statics plus active doors/pushwalls, add actor/player interactions, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:00 CDT

Action taken:

- Broadened live runtime-scene coverage by sending five real WL6 map-0 runtime sprite refs through `wl_render_runtime_door_camera_scene_view`.
- The test decodes required VSWAP sprite surfaces locally, renders against the mutable `wl_game_model::tilemap`, verifies the closed door at `(32,57)` emits door page `99`, then clears that door center and verifies the ray continues to later wall page `15`.
- Asserted live runtime-ref scene hashes: closed-door composition `0x21495346`, open-door composition `0x2e4660d2`.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add a small headless live gameplay tick that combines movement, pickup, use, door/pushwall progression, palette shifts, and live scene rendering; or broaden multi-map runtime scenes.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:04 CDT

Action taken:

- Added `wl_step_live_tick`, a pure-C orchestration seam for a future frame update.
- The helper runs player motion/pickup probing, optional use-button dispatch, door progression, pushwall progression, and palette-shift advancement in one deterministic headless call.
- Tests cover a movement tick that picks up food, heals the player, deactivates the runtime static, and emits a white palette shift; and a use tick that starts a pushwall and advances it across its first block boundary.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Feed `wl_step_live_tick` output into the live scene/palette upload path, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:08 CDT

Action taken:

- Connected `wl_step_live_tick` palette output to the existing shifted texture upload metadata path.
- A headless test now runs a food-pickup live tick, consumes its white palette-shift result with `wl_describe_palette_shifted_texture_upload`, and verifies the selected white palette plus RGBA expansion hash `0x93adda7f`.
- This ties gameplay-triggered palette flashes to the future SDL3 texture-upload boundary without adding an SDL dependency.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Feed live tick state changes into a combined live scene render, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:12 CDT

Action taken:

- Connected `wl_step_live_tick` state changes directly to live door-aware scene rendering.
- A headless test now runs a keyed use-button tick against a closed door, verifies the tick opens the door and clears the live collision tile, then renders the resulting open doorway through `wl_render_runtime_door_camera_scene_view`.
- The tick-driven scene is asserted against the existing deterministic open-door scene hash, proving gameplay tick output reaches renderer-facing wall+sprite presentation state without SDL.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden live tick + scene coverage to pushwalls/static pickups, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:16 CDT

Action taken:

- Broadened live tick scene coverage from door state to pushwall state.
- A headless test now runs a use-button live tick that starts a pushwall and advances it across its first block boundary, verifies the mutable tilemap carries the `37 | 0xc0` moving marker, then renders that state through `wl_render_runtime_door_camera_scene_view`.
- The tick-driven pushwall scene is asserted against the existing deterministic moving-pushwall occlusion hash, proving input-style tick state reaches renderer-facing wall+sprite output without SDL.
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden live tick + scene coverage to static pickup/removal, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:20 CDT

Action taken:

- Broadened live tick scene coverage from doors/pushwalls to static pickup removal.
- A headless test now collects an active bonus static sprite ref, decodes its local VSWAP surface, renders the active static through `wl_render_runtime_door_camera_scene_view`, then runs `wl_step_live_tick` to pick it up and deactivate it.
- A fresh scene-ref collection returns zero refs after pickup, and the no-static scene renders to a distinct deterministic hash (`0x7e68266c` active vs `0xc928b202` picked).
- Updated README, runtime/VSWAP research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden multi-map live scene coverage, add actor/player interaction events, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:07 CDT

Action taken:

- Added a deterministic actor/player interaction seam for dog bite damage.
- `wl_try_actor_bite_player` checks the original `T_Bite` proximity window, applies the original chance-roll threshold shape, derives damage from `damage_roll >> 4`, and routes hits through `wl_apply_player_damage`.
- Headless tests cover in-range damage, out-of-range no-op, chance miss, baby-difficulty scaling, red palette-shift output, and non-dog rejection.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add deterministic actor shooting/projectile damage seams, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:11 CDT

Action taken:

- Added a deterministic actor shooting damage seam modeled on original `T_Shoot`.
- `wl_try_actor_shoot_player` gates on area/line-of-sight, computes original distance-based hit chance from running/visibility state, applies SS/boss distance advantage, derives close/medium/far damage from `damage_roll`, and routes hits through `wl_apply_player_damage`.
- Headless tests cover inactive area, blocked line of sight, chance miss, far guard damage, SS adjusted-distance medium damage with baby scaling, close guard damage, and non-shootable rejection.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Add deterministic projectile damage/movement seams, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:15 CDT

Action taken:

- Added a deterministic projectile movement/collision/damage seam modeled on original `ProjectileTryMove` / `T_Projectile`.
- `wl_step_projectile` advances fixed-point projectile state, applies the original positive one-tile movement clamp shape, checks `PROJSIZE` runtime tilemap collision, detects player impacts with `PROJECTILESIZE`, maps needle/rocket/hrocket/spark/fire damage from `damage_roll`, and routes hits through `wl_apply_player_damage`.
- Headless tests cover wall impact/removal, needle damage, rocket baby-difficulty scaling, fire projectile travel before impact, tile updates, and inactive-projectile rejection.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect actor damage/projectile seams into a broader live tick, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:19 CDT

Action taken:

- Connected projectile damage into a broader live tick orchestration path.
- Added `wl_step_live_projectile_tick`, which mirrors the existing live tick motion/use/door/pushwall sequence, optionally steps an active projectile, then advances palette state after projectile damage is applied.
- Headless tests verify a live tick needle projectile impact damages the player, removes the projectile, and returns a red palette shift in the same frame; the null-projectile path also remains valid.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-projectile-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect actor shooting/bite events into live tick orchestration, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:30 CDT

Action taken:

- Connected actor bite/shoot damage into a broader live tick orchestration path.
- Added `wl_step_live_actor_tick`, which mirrors the live motion/use/door/pushwall sequence, dispatches deterministic dog bites or shootable actor attacks, then advances palette state after damage is applied.
- Headless tests verify same-frame dog-bite and guard-shot damage, red palette results, and the null-actor no-op path.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-actor-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Combine live actor/projectile tick orchestration, broaden multi-map live scene coverage, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:40 CDT

Action taken:

- Combined actor and projectile damage seams into one deterministic live combat tick.
- Added `wl_step_live_combat_tick`, which runs live motion/use/door/pushwall state, applies an optional actor bite/shoot attack, applies an optional active projectile step, then advances palette state once after all damage for the frame.
- Headless tests verify dog-bite plus needle-projectile damage in one tick, cumulative health/damage counters, same-frame red palette selection, projectile removal, and the null-combat no-op path.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-combat-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden multi-map live scene coverage, start deterministic actor state progression, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 02:50 CDT

Action taken:

- Connected the combined live combat tick result to the existing palette-shifted upload descriptor seam.
- Added headless coverage where one `wl_step_live_combat_tick` applies dog-bite plus needle-projectile damage, returns the summed red palette shift, then feeds that result into `wl_describe_palette_shifted_texture_upload`.
- Tests assert the selected red-shift palette pointer, palette hash, RGBA expansion hash, cumulative damage counter, and projectile removal without SDL or committed asset bytes.
- Updated README, runtime/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-combat-upload tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Broaden multi-map live scene coverage, start deterministic actor state progression, or add an SDL3 presentation seam once SDL3 is available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 03:00 CDT

Action taken:

- Added deterministic actor hitpoint/damage/kill state seams based on original `starthitpoints`, `DamageActor`, and `KillActor` behavior.
- Added `wl_init_actor_combat_state` and `wl_apply_actor_damage` to model difficulty-scaled starting hitpoints, double damage before attack mode, attack-mode start on nonlethal hits, kill score awards, shootable/alive clearing, and representative item drops.
- Headless tests cover guard pain/kill/drop, SS machinegun drop and extra-life threshold crossing, boss key drop, and dead-guard rejection.
- Updated README, runtime/gameplay research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-actor-damage-state tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect actor kill/drop state into live combat tick/runtime statics, broaden multi-map live scene coverage, or start deterministic actor state progression.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 03:10 CDT

Action taken:

- Connected actor kill/drop results into runtime static spawning.
- Added `wl_spawn_actor_drop_static`, which turns killed-actor drop metadata from `wl_apply_actor_damage` into an active runtime bonus static at the actor tile.
- Headless tests verify guard clip drops spawn as type/source metadata and can be picked up for ammo, SS machinegun drops spawn as machinegun statics, and no-drop dog kills leave static counts unchanged.
- Updated README, runtime/gameplay research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make clean test
```

Result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-actor-drop-static tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state/hash assertions are committed.

Next likely move:

- Connect actor kill/drop static spawning into live combat tick results, render spawned drops through scene refs, or start deterministic actor state progression.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 03:20 CDT

Action taken:

- Connected actor damage/kill/drop state into a live tick orchestration path.
- Added `wl_live_actor_damage_tick_result` and `wl_step_live_actor_damage_tick`, which run live motion/use/door/pushwall progression, apply deterministic actor damage, spawn actor drops into runtime statics when killed, then advance palette state.
- Headless tests verify a live guard kill awards score, clears actor alive state, spawns a clip drop static at the actor tile, reports the spawned static index, and preserves the null-actor no-op path.
- Updated README, runtime/gameplay research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-actor-damage-drop tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Render live-spawned actor drops through scene refs, fold actor damage into broader live combat orchestration, or start deterministic actor state progression.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 03:30 CDT

Action taken:

- Connected live-spawned actor drops to renderer-facing scene refs and door-aware scene rendering.
- Extended the headless scene test so `wl_step_live_actor_damage_tick` kills a guard, spawns a clip drop static, `wl_collect_scene_sprite_refs` emits the drop sprite ref, local VSWAP cache decoding builds its sprite surface, and `wl_render_runtime_door_camera_scene_view` renders the drop into a deterministic scene.
- Assertions cover spawned static metadata, source/vswap sprite mapping, and scene hash `0x707dbe4e` without committing decoded asset bytes.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-actor-drop-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash/state assertions are committed.

Next likely move:

- Fold actor damage/drop spawning into the broader live combat tick, add death-state actor progression, or broaden multi-map runtime scene coverage.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 03:40 CDT

Action taken:

- Folded actor damage/drop spawning into a fuller live combat orchestration path.
- Added `wl_live_full_combat_tick_result` and `wl_step_live_full_combat_tick`, which run one live motion/use/door/pushwall update, optionally damage/kill a target actor and spawn its drop, optionally apply actor bite/shoot damage and projectile damage to the player, then advance palette state once for the frame.
- Headless tests verify a same-frame guard kill/drop, dog bite, needle projectile hit, cumulative red palette selection, player score/health changes, projectile removal, actor alive clearing, spawned drop metadata, and a null-combat no-op path.
- Updated README, runtime/gameplay/palette research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-full-combat tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Add deterministic actor death-state progression, render full-combat-spawned drops in a scene, or broaden multi-map runtime scene coverage.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 03:50 CDT

Action taken:

- Added deterministic actor death-state progression metadata based on original `WL_STATE.C::KillActor` and `WL_ACT2.C` death `statetype` chains.
- Added `wl_actor_death_state`, `wl_actor_death_step_result`, `wl_start_actor_death_state`, and `wl_step_actor_death_state`.
- Headless tests verify guard death tics and final dead sprite, dog looping dead frame timing, boss death final sprite, one-shot death-scream flag, and rejection of invalid/dead-guard state setup.
- Updated README, runtime/VSWAP/gameplay research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-actor-death-state tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Render death-state actor sprites through scene refs, fold death-state metadata into live full combat output, or broaden multi-map runtime scene coverage.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 04:00 CDT

Action taken:

- Connected actor death-state metadata to renderer-facing scene refs and door-aware scene rendering.
- Added `wl_build_actor_death_scene_ref`, which converts a killed actor plus active death-state sprite into a `wl_scene_sprite_ref` with world position and VSWAP chunk metadata.
- Extended the headless render test so a killed guard death frame decodes through the local VSWAP sprite surface cache and renders in `wl_render_runtime_door_camera_scene_view`; committed proof is scene hash `0x2e8b4819`, not decoded sprite bytes.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-actor-death-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash/state assertions are committed.

Next likely move:

- Fold death-state scene refs into live full-combat output, add death-state final-frame replacement for killed actors, or broaden multi-map runtime scene coverage.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 04:10 CDT

Action taken:

- Folded actor death-state metadata and scene-ref output into the full live combat tick.
- Extended `wl_live_full_combat_tick_result` so a killed target actor now returns `actor_death`, `actor_death_ref`, `death_started`, and `death_ref_built` alongside score/drop/palette/projectile results.
- `wl_step_live_full_combat_tick` now starts death-state progression and builds the first renderer-facing death scene ref in the same headless frame as actor kill/drop spawning.
- Headless tests verify same-frame guard kill/drop/death ref, VSWAP source/chunk metadata, bite/projectile damage, cumulative red palette selection, and null-combat no-op flags.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-full-combat-death tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Add final-death-frame replacement in runtime actor refs, render full-combat death refs directly in a scene, or broaden multi-map runtime scene coverage.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 04:20 CDT

Action taken:

- Rendered full-combat death refs directly through the headless scene path.
- Extended the WL6 render test so `wl_step_live_full_combat_tick` kills a guard, returns the initial death scene ref, decodes that referenced VSWAP sprite surface locally, and renders it through `wl_render_runtime_door_camera_scene_view`.
- The full-combat death-ref scene matches the independently rendered death-state scene hash `0x2e8b4819`, proving the live combat result itself can drive presentation input without decoded asset bytes in git.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-full-combat-death-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash/state assertions are committed.

Next likely move:

- Add final-death-frame replacement in runtime actor refs, broaden multi-map runtime scene coverage, or deepen actor AI/state progression.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 04:30 CDT

Action taken:

- Added final death-frame replacement for runtime actor scene refs.
- Extended `wl_actor_desc` with an optional scene-source override consumed by `wl_collect_scene_sprite_refs`, leaving default actor/static refs unchanged.
- Added `wl_apply_actor_death_final_frame`, which applies a finished `wl_actor_death_state` to a runtime actor slot by making it inert/non-shootable and overriding its renderer-facing sprite source to the final death frame.
- Headless tests verify a live guard ref changes from standing guard source `50` to final dead-guard source `95` / VSWAP chunk `201`, with invalid unfinished/out-of-range death applications rejected.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-actor-death-final-frame tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Broaden multi-map runtime scene coverage, add a small actor-state progression seam, or add SDL3 presentation once SDL3 development files are available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 04:40 CDT

Action taken:

- Added a live actor death animation tick/finalization seam.
- Added `wl_live_actor_death_tick_result` and `wl_step_live_actor_death_tick`, which advance an active `wl_actor_death_state`, emit a renderer-facing death scene ref for the current frame, and apply the final corpse-frame override to the runtime actor slot when the sequence finishes.
- Headless tests verify guard death progresses from source `91` to `92`, then finishes at final source `95` / VSWAP chunk `201`, and that normal scene-ref collection afterwards emits the persistent corpse ref for the same actor slot.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-actor-death-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Broaden multi-map runtime scene coverage, connect death ticking into full-combat frame orchestration, or add broader actor AI/state progression.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 04:50 CDT

Action taken:

- Connected active death animation ticking into the full live combat orchestration boundary.
- Added `wl_live_full_combat_death_tick_result` and `wl_step_live_full_combat_death_tick`, which run the full combat frame transition and optionally advance an active actor death state in the same headless frame.
- The new seam returns both normal full-combat results and death-tick output, including renderer-facing death refs and final corpse-frame application to the runtime actor slot.
- Headless tests verify a guard killed by full combat can have its active death state advanced/finalized through the combined wrapper, preserving the spawned drop while applying final source `95` / VSWAP chunk `201` to actor model index `9`.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-full-combat-death-tick tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Broaden multi-map runtime scene coverage, render combined full-combat/death-tick final refs in-scene, or add broader actor AI/state progression.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 05:00 CDT

Action taken:

- Rendered the combined full-combat/death-tick final corpse ref through the headless scene path.
- Extended the WL6 renderer test so a guard killed by full combat starts death-state output, then `wl_step_live_full_combat_death_tick` finalizes the active death state to source `95` / VSWAP chunk `201` and the returned death ref is decoded/rendered through `wl_render_runtime_door_camera_scene_view`.
- Added deterministic scene hash `0x81c10dcf` for the combined full-combat/death-tick final corpse scene.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-live-full-combat-death-tick-scene tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash/state assertions are committed.

Next likely move:

- Broaden multi-map runtime scene coverage, connect more actor AI/state progression, or add SDL3 presentation once SDL3 development files are available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 05:10 CDT

Action taken:

- Broadened actor final-death-frame runtime ref coverage across enemy classes.
- Extended headless tests beyond the guard path to finalize officer, SS, dog, mutant, and boss death states through `wl_apply_actor_death_final_frame` and `wl_collect_scene_sprite_refs`.
- Verified final renderer-facing source/VSWAP metadata for officer `284/390`, SS `183/289`, dog `134/240`, mutant `233/339`, and boss `303/409`, without committing decoded asset bytes.
- Updated README, runtime/VSWAP/graphics research notes, and this state file.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-actor-death-final-broaden tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/state assertions are committed.

Next likely move:

- Broaden multi-map runtime scene coverage, connect more actor AI/state progression, or add SDL3 presentation once SDL3 development files are available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.



## Cycle 2026-04-25 07:30 CDT

Action taken:

- Broadened WL6 multi-map runtime scene-ref coverage across additional boss and secret maps.
- Extended headless model assertions to cover `Wolf3 Boss`, `Wolf4 Boss`, `Wolf6 Boss`, and mixed `Wolf6 Secret` boss/ghost/mutant refs in addition to the existing `Wolf2 Boss` and `Wolf3 Secret` coverage.
- Added deterministic scene-ref descriptor hashes `0x713b74e4`, `0x7a3bd88c`, `0x2b8bf08d`, and `0x59f0ca20` without storing decoded sprite bytes.
- Noted that `Wolf1 Boss` and `Wolf5 Boss` still need a small unknown-info-tile classification pass before they can join zero-unknown model assertions.
- Updated README and runtime/VSWAP research notes.

Verification:

```bash
cd source/modern-c-sdl3
make test
```

Result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/runtime-map-boss-scene-broaden tests passed for game-files/base
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data; only metadata/hash/state assertions are committed.
- Removed scratch probe build files and kept normal build artifacts ignored.

Next likely move:

- Fill the remaining unknown boss info-tile classifications, deepen actor AI/state progression, or add SDL3 presentation once SDL3 development files are available.

Blockers: none for headless work; SDL3 presentation cannot be verified here until SDL3 development files are available.


## Cycle 2026-04-25 07:47 CDT

Action taken:

- Updated autopilot policy from ET's feedback: ordinary blockers should trigger pivots/research/splitting rather than stopping, and repeated same-issue failures should usually mean many materially different attempts before escalation.
- Made `scripts/wolf3d_autopilot_supervisor.py` configurable for cycle thinking, summary thinking, fast-mode requests, model preference rotation, and weekly usage-budget guarding.
- Defaulted cycle and summary thinking to `low` and fast mode to off.
- Added default model preference rotation between `openai-codex/gpt-5.5` and `anthropic/claude-opus-4.7`; current CLI builds without `openclaw agent --model` receive the model as a cycle prompt hint, and future CLI builds with `--model` will get the flag automatically.
- Added `openclaw status --usage --json` weekly guardrails: infer reset/start from the provider usage window, compare `usedPercent` against a linear week curve plus slack, and pause/recheck when the autopilot is ahead of budget.
- Updated supervisor docs with the new knobs and budget model.

Verification:

```bash
python3 -m py_compile scripts/wolf3d_autopilot_supervisor.py
scripts/wolf3d_autopilot_supervisor.py --help
cd source/modern-c-sdl3 && make test
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data.
- Usage/model state remains runtime state under ignored `state/` files.

Next likely move:

- Run the supervisor with the new low-thinking/default-budget settings, or continue the port by filling remaining unknown boss info-tile classifications.

Blockers: none for headless work. Direct model switching depends on OpenClaw CLI support for `openclaw agent --model`; until then the supervisor records and injects model preference hints.


## Cycle 2026-04-25 07:55 CDT

Action taken:

- Refined the usage-budget guard from a global provider gate into a provider-aware model selector.
- The supervisor now infers each model's provider from the model prefix before `/`, checks that provider's weekly budget, skips over-budget providers, and selects the next available model in rotation.
- If every configured provider is over budget, the supervisor pauses only until the earliest provider should be back inside the configured budget curve, then re-checks.
- Updated supervisor docs with examples for multi-provider rotation including Codex, Anthropic, and Z.ai-style model prefixes.

Verification:

```bash
python3 -m py_compile scripts/wolf3d_autopilot_supervisor.py
scripts/wolf3d_autopilot_supervisor.py --help
cd source/modern-c-sdl3 && make test
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data.

Next likely move:

- Add real model IDs for any additional providers once confirmed by the OpenClaw runtime, then run the supervisor so it can skip exhausted providers automatically.

Blockers: none for headless work. Direct model switching still depends on OpenClaw CLI support for `openclaw agent --model`; until then selected models remain prompt-level preferences.


## Cycle 2026-04-25 08:11 CDT

Action taken:

- Updated provider usage budgeting to support provider-specific usage windows instead of assuming every provider is weekly.
- Added `--usage-provider-windows` overrides, defaulting to `zai:Monthly`, so Z.ai's monthly quota window is paced monthly while Codex/Anthropic can continue using the default `Week` window.
- Added automatic budget-period inference from the usage window label: `Monthly` uses a 30-day budget period, `Week` uses 7 days, and labels like `5h` use the matching hourly period.
- Added Z.ai to the default model rotation as `zai/glm-5.1`.
- Updated supervisor docs with monthly Z.ai examples and provider-window override guidance.

Verification:

```bash
python3 -m py_compile scripts/wolf3d_autopilot_supervisor.py
scripts/wolf3d_autopilot_supervisor.py --help
cd source/modern-c-sdl3 && make test
```

Safety/legal checks:

- Did not modify `source/original/`.
- Did not add or commit proprietary game data.

Next likely move:

- Run one dry/limited supervisor cycle once model switching support is confirmed, or continue headless port work using the updated provider-aware budget policy.

Blockers: direct model switching still depends on OpenClaw CLI support for `openclaw agent --model`; selected models are prompt-level preferences until that exists.
