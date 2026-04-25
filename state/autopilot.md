# Autopilot State

Status: active

## Current Phase

Fixed-height wall strip scaling is implemented and passing on headless Linux for WL6 and optional SOD. Next phase should use it in a tiny viewport/raycast smoke test or add palette/texture-upload metadata.

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
- `docs/research/runtime-map-model.md` records the pure C runtime model seam, door-area connectivity descriptors, descriptor assertions, and verification output.
- `docs/research/vswap-directory.md` records full VSWAP chunk-directory parsing, bounded chunk-read hashes, wall-page metadata/surface/column-sampler/scaler assertions, sprite shape metadata assertions, sprite post-command metadata assertions, range/count assertions, and verification output.
- `docs/research/graphics-huffman.md` records VGAHEAD/VGADICT/VGAGRAPH parsing, pure C Huffman expansion, STRUCTPIC picture-table metadata, planar-to-indexed surface conversion, renderer-facing indexed-surface descriptors, SDL-free indexed blitting, WL6/SOD graphics chunk smoke assertions, and verification output.

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
11. `VGAHEAD`/`VGADICT`/`VGAGRAPH` Huffman smoke test. **Done for representative WL6/SOD chunks.**
12. `STRUCTPIC` picture metadata interpretation. **Done for WL6/SOD.**
13. Renderer-facing indexed-surface seam. **Initial planar-to-indexed conversion done for WL6/SOD.**
14. Surface metadata/type layer. **Done for WL6/SOD indexed picture surfaces.**
15. SDL-free blit/composite smoke test. **Done for representative WL6 surfaces.**
16. Wall-page metadata decoding and row-major indexed surface seam. **Done for representative WL6/SOD walls.**
17. Raycaster texture-column sampler. **Done for representative WL6/SOD walls.**
18. Fixed-height wall strip scaler. **Done for representative WL6/SOD walls.**
19. Tiny viewport/raycast smoke test or palette/texture-upload metadata seam.

## Next Likely Move

Build a tiny viewport/raycast smoke test or add palette/texture-upload metadata.

Recommended next commit:

- use sampled/scaled wall strips in a tiny deterministic viewport/raycast smoke test;
- or add palette/texture-upload metadata that can later connect `wl_indexed_surface` to SDL3 textures.

The current harness already verifies WL6 file sizes, `MAPHEAD.WL6` RLEW tag `0xabcd`, map 0 offset/header/name/dimensions, `VSWAP.WL6` header/directory values, bounded chunk-read hashes, representative wall/sprite shape metadata, sprite post-command metadata, VGA graphics Huffman chunk hashes, STRUCTPIC dimensions, indexed-surface hashes/descriptors, indexed blit canvas hashes, wall-page metadata/surface hashes, wall texture-column sampler hashes, wall strip scaler canvas hashes, optional SOD metadata, Carmack/RLEW helper behavior, WL6 map 0 plane hashes/counts, WL6 map 0 semantic classification counts, a WL6 map 0 `SetupGameLevel`-style runtime model, and door-area connectivity descriptors.

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
