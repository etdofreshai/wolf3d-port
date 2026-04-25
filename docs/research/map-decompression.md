# Map Decompression Notes

Research/implementation cycle: 2026-04-24 22:26-22:45 CDT  
Scope: faithful pure C Carmack/RLEW expansion and WL6 map-plane decode under `source/modern-c-sdl3`.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/ID_CA.C::CAL_CarmackExpand`
- `source/original/WOLFSRC/ID_CA.C::CA_RLEWexpand`
- `source/original/WOLFSRC/ID_CA.C::CA_CacheMap`

Important behavior preserved:

- Carmack expansion works in 16-bit little-endian words.
- `NEARTAG = 0xa7`, `FARTAG = 0xa8` are high-byte markers.
- A tag with zero count escapes a literal word containing that tag high byte and consumes one following byte.
- A near copy uses a one-byte backward word offset from the current output position.
- A far copy uses a two-byte absolute word offset from the output buffer start.
- Map plane chunks start with a two-byte Carmack expanded length.
- The Carmack-expanded RLEW stream also starts with a two-byte final expanded length; original `CA_CacheMap` skips that word before RLEW expansion.

## Implemented seam

Added these APIs to `wl_assets`:

- `wl_carmack_expand(...)`
- `wl_rlew_expand(...)`
- `wl_read_map_plane(...)`

`wl_read_map_plane` reads one compressed plane from `GAMEMAPS`, expands Carmack data, validates the embedded RLEW expanded length is `8192` bytes, then expands to `64 * 64` 16-bit words.

## Test coverage

`source/modern-c-sdl3/tests/test_assets.c` now includes:

- Synthetic Carmack tests covering near-copy and escaped near/far tag literals.
- Synthetic RLEW test covering a repeated run.
- WL6 map 0 plane 0/1/2 decode tests using local `game-files/base` data.

Committed assertions are only metadata/hashes/counts, not map bytes:

| Plane | FNV-1a over little-endian words | Key assertions |
| --- | --- | --- |
| 0 wall plane | `0x5940a18e` | 4096 nonzero words, tile `1` count 2331, tile `8` count 230 |
| 1 object plane | `0xacf24351` | 183 nonzero words, zero count 3913, value `37` count 30 |
| 2 info plane | `0xbcc31dc5` | all zeros |

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
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset metadata tests passed for game-files/base
```

## Next step

Use decoded planes to build the first map semantic characterization: classify wall tiles, doors, pushwalls, static objects, actor starts, and player start metadata for WL6 map 0. Keep this pure C/headless before adding SDL3 presentation.
