# VSWAP Directory Notes

Research/implementation cycle: 2026-04-24 22:41-23:00 CDT  
Scope: full VSWAP chunk-directory parsing for WL6 and optional SOD, without decoding proprietary chunk bytes.

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
asset/decompression/semantics/model/vswap tests passed for game-files/base
```

## Next step

Use the VSWAP directory to add first chunk read smoke tests and/or begin wall/sprite shape metadata decoding. Keep assertions to lengths, stable hashes, and decoded metadata rather than committing chunk bytes.
