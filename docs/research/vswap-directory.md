# VSWAP Directory Notes

Research/implementation cycle: 2026-04-24 22:41-23:00 CDT  
Scope: VSWAP chunk-directory, bounded reads, wall-page metadata/surface conversion, wall texture-column sampling, fixed-height wall strip scaling, tiny wall-strip viewport composition, map-derived wall-hit descriptors, cardinal/fixed-point/DDA ray stepping, and metadata-only sprite shape parsing for WL6 and optional SOD, without committing proprietary chunk bytes.

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
- `source/original/WOLFSRC/WL_SCALE.C::ScaleShape`
  - treats each visible sprite column as a list of 6-byte commands: end pixel*2, corrected top/source offset, start pixel*2.
  - a zero end word terminates each column's command list.

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
- validates/counts sprite post-command streams without retaining pixel data;
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
- wall metadata for chunk `63`: colors `26..223`, `31` unique colors, row-major indexed hash `0x5b4d4c38`, sampled column hash `0x8a859220`, combined scaled-strip/viewport canvas hash `0x0b200118`; map-derived viewport hash `0x7ffb21c0`; cardinal/fixed-ray viewport hash `0xa4c9e6e1`, DDA mixed-ray viewport hash `0xae40b70c`
- wall metadata for chunk `105`: colors `0..31`, `11` unique colors, row-major indexed hash `0x66874cf5`
- sprite metadata for chunk `106`: `64x64`, left/right pixels `4..58`, `55` visible columns, first/last column offsets `800/1298`
- sprite post metadata for chunk `106`: `66` posts, `55` column terminators, `1..2` posts/column, span range `2..40`, source-offset range `108..782`, total post span `1372`
- sprite post metadata for chunk `107`: `85` posts, `62` column terminators, `1..3` posts/column, max span `36`, source-offset range `113..904`, total post span `1586`

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
asset/decompression/semantics/model/vswap/dda-ray tests passed for game-files/base
```

## Cycle update: chunk reads and shape metadata

Added `wl_read_vswap_chunk`, a bounded read helper that validates chunk index, sparse entries, output buffer size, and directory/file bounds before reading chunk bytes into caller-provided memory. Tests assert lengths and stable FNV-1a hashes for representative wall, sprite, sound, and final chunks without committing bytes.

Added `wl_decode_vswap_shape_metadata` for safe metadata-only interpretation of representative wall/sprite chunks. Wall chunks assert the canonical `64x64` raw page shape; sprite chunks assert `t_compshape`-style left/right bounds, packed column-offset table metadata, and post-command list counts/ranges.

Added `wl_decode_wall_page_metadata`, `wl_decode_wall_page_to_indexed`, and `wl_decode_wall_page_surface` for raw VSWAP wall pages. These functions preserve the original column-major `PM_GetPage(wallpic) + texture` addressing model while providing row-major indexed surfaces for future SDL3 texture/raycaster seams. Added `wl_sample_wall_page_column` and `wl_sample_indexed_surface_column` so raycaster-oriented code can sample a canonical 64-byte texture column and verify it matches the row-major surface view. Added `wl_scale_wall_column_to_surface`, a pure C fixed-height wall strip scaler that mirrors the original `BuildCompScale` source-pixel run distribution while writing to linear indexed surfaces. Added `wl_render_wall_strip_viewport`, a tiny SDL-free composition seam that renders an ordered set of wall-strip commands into a caller-owned indexed viewport. Added `wl_build_map_wall_hit` and `wl_wall_hit_to_strip` to turn decoded map-plane wall tiles into horizontal/vertical page-index + texture-offset descriptors before viewport composition. Added `wl_cast_cardinal_wall_ray`, a first map stepping helper that walks north/east/south/west from a tile and emits the first solid-wall hit descriptor. Added `wl_cast_fixed_cardinal_wall_ray`, which accepts 16.16 world origins and derives original-style texture offsets from the perpendicular intercept coordinate. Added `wl_cast_fixed_wall_ray`, which steps arbitrary 16.16 direction vectors across vertical/horizontal map-grid crossings, derives the hit-side texture column from the fixed-point intercept coordinate, and feeds the existing wall-hit/viewport seam.

## Cycle update: sprite post metadata

Extended sprite metadata decoding to walk each visible column's post-command stream, validate start/end pixel*2 ranges and zero terminators, and record aggregate post counts, post-span ranges, source-offset ranges, and posts-per-column bounds. This gives the renderer path a deterministic oracle for compressed sprite layout without storing or committing proprietary pixel bytes.

## Next step

Build on the fixed DDA helper with distance/height projection and screen-column batches, or add palette/texture-upload metadata before SDL3 presentation. Keep assertions to decoded metadata and stable hashes rather than committing chunk bytes.
