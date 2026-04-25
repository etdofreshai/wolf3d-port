# Asset Metadata Harness Notes

Research/implementation cycle: 2026-04-24 22:22-22:35 CDT  
Scope: first runnable modern C tests under `source/modern-c-sdl3`.

## Implemented seam

Added a pure C `wl_assets` module as the first platform-independent asset boundary for the modern C + SDL3 port.

Files:

- `source/modern-c-sdl3/include/wl_assets.h`
- `source/modern-c-sdl3/src/wl_assets.c`
- `source/modern-c-sdl3/tests/test_assets.c`
- `source/modern-c-sdl3/Makefile`

The implementation intentionally avoids SDL for now. It uses normal C stdio and explicit little-endian readers so it can run on headless Linux and form the base for later SDL3 presentation/input/audio boundaries.

Audio metadata now includes a small `AUDIOT` descriptor seam: representative PC speaker, AdLib, empty digital, and IMF music chunks are classified by original WL6 chunk ranges, expose an explicit empty flag, raw size, declared length, sound priority where present, and payload offsets/sizes. This keeps future decode/playback work deterministic without committing local proprietary audio data.

## Current test coverage

`make test` builds and runs `build/test_assets` from `source/modern-c-sdl3`.

Default data path:

- `game-files/base`

Override:

- `WOLF3D_DATA_DIR=/path/to/WL6/data make test`

WL6 assertions currently verify:

- Required file sizes for `MAPHEAD.WL6`, `GAMEMAPS.WL6`, `VSWAP.WL6`, `VGAHEAD.WL6`, `VGADICT.WL6`, `VGAGRAPH.WL6`, `AUDIOHED.WL6`, and `AUDIOT.WL6`.
- `MAPHEAD.WL6` size `402`, RLEW tag `0xabcd`, first map offset `2250`.
- `GAMEMAPS.WL6` map 0 header at that offset: name `Wolf1 Map1`, dimensions `64x64`, plane starts `(11, 1445, 2240)`, plane lengths `(1434, 795, 10)`.
- `VSWAP.WL6` header: `ChunksInFile=663`, `PMSpriteStart=106`, `PMSoundStart=542`, first offsets `(4096, 8192, 12288, 16384, 20480)`.

Optional SOD assertions run when `game-files/base/m1` exists:

- `MAPHEAD.SOD` size `402`, RLEW tag `0xabcd`, first map offset `2097`.
- `GAMEMAPS.SOD` map 0 name `Tunnels 1`, dimensions `64x64`.
- `VSWAP.SOD` header `666/134/555`.

## Verification evidence

Command:

```bash
cd source/modern-c-sdl3
make test
```

Observed result:

```text
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset metadata tests passed for game-files/base
```

## Legal/asset boundary

The tests read local proprietary data from `game-files/`, but no proprietary bytes are committed. Root `.gitignore` now ignores `source/modern-c-sdl3/build/`, while `game-files/.gitignore` continues to ignore game assets.

## Next step

Implement pure C Carmack expansion and RLEW expansion, then decode WL6 map 0 planes into 4096-word arrays. Use hashes/counts as assertions rather than committing map bytes.
