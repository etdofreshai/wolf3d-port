# Graphics Huffman Notes

Research/implementation cycle: 2026-04-24 22:53-23:15 CDT  
Scope: `VGAHEAD`/`VGADICT`/`VGAGRAPH` header parsing and Huffman smoke decoding for WL6 and optional SOD, without committing proprietary graphics bytes.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/ID_CA.C::GRFILEPOS`
  - reads 3-byte graphics offsets when `THREEBYTEGRSTARTS` is enabled.
  - treats `0xffffff` as sparse `-1`.
- `source/original/WOLFSRC/ID_CA.C::CAL_SetupGrFile`
  - loads `VGADICT.*` as 255 Huffman nodes.
  - loads `(NUMCHUNKS + 1)` 3-byte entries from `VGAHEAD.*`.
  - decodes `STRUCTPIC` into the picture table during setup.
- `source/original/WOLFSRC/ID_CA.C::CAL_HuffExpand`
  - uses node 254 as the Huffman head node.
  - consumes bits least-significant-bit first from each compressed byte.
  - values `< 256` are literal output bytes; values `>= 256` reference `table[value - 256]`.
- `source/original/WOLFSRC/GFXV_WL6.H`
  - `NUMCHUNKS = 149`, `NUMPICS = 132`, `STRUCTPIC = 0`, `STARTFONT = 1`, `STARTPICS = 3`, `STARTTILE8 = 135`, `STARTEXTERNS = 136`.
- `source/original/WOLFSRC/GFXV_SOD.H`
  - `NUMCHUNKS = 169`, `NUMPICS = 147`, `STRUCTPIC = 0`, `STARTFONT = 1`, `STARTPICS = 3`, `STARTTILE8 = 150`, `STARTEXTERNS = 151`.

## Implemented seam

Added graphics APIs/types to `wl_assets`:

- `WL_GRAPHICS_MAX_CHUNKS`
- `WL_HUFFMAN_NODE_COUNT`
- `wl_huffman_node`
- `wl_graphics_header`
- `wl_read_graphics_header(...)`
- `wl_read_huffman_dictionary(...)`
- `wl_huff_expand(...)`
- `wl_read_graphics_chunk(...)`

The implementation:

- parses 3-byte `VGAHEAD` entries and sparse sentinel offsets;
- parses the 255-node `VGADICT` Huffman table;
- ports the original LSB-first Huffman traversal as a pure C function;
- reads explicit-size `VGAGRAPH` chunks and decodes them into caller-provided buffers;
- records only sizes/counts/hashes in tests and docs, not graphics bytes.

## WL6 committed assertions

`game-files/base/VGAHEAD.WL6` / `VGADICT.WL6` / `VGAGRAPH.WL6`:

- `VGAHEAD.WL6` entries: `150` (`149` chunks plus sentinel)
- `VGAHEAD.WL6` size: `450`
- first offsets: `0`, `354`, chunk 3 offset `9570`, final sentinel `275774`
- decoded chunk `0` (`STRUCTPIC`): compressed `354`, expanded `528`, FNV-1a `0x0a6f459a`
- decoded chunk `1` (`STARTFONT`): compressed `3467`, expanded `8300`, FNV-1a `0xdb48ce2b`
- decoded chunk `87` (`TITLEPIC`): compressed `45948`, expanded `64000`, FNV-1a `0x01643ebc`
- decoded chunk `134` (`GETPSYCHEDPIC`): compressed `5127`, expanded `10752`, FNV-1a `0xeb393cc0`

## Optional SOD committed assertions

When `game-files/base/m1` is present:

- `VGAHEAD.SOD` entries: `170` (`169` chunks plus sentinel)
- `VGAHEAD.SOD` size: `510`
- first offsets: `0`, `383`, final sentinel `947979`
- decoded chunk `0` (`STRUCTPIC`): compressed `383`, expanded `588`, FNV-1a `0x43f617ea`
- decoded chunk `1` (`STARTFONT`): compressed `4448`, expanded `8300`, FNV-1a `0xdb48ce2b`
- decoded chunk `3`: compressed `42248`, expanded `64000`, FNV-1a `0x3a6afac3`
- decoded chunk `149`: compressed `6243`, expanded `10752`, FNV-1a `0xeb393cc0`

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
asset/decompression/semantics/model/vswap/vga-huffman tests passed for game-files/base
```

## Next step

Use decoded graphics metadata to interpret `STRUCTPIC` picture dimensions and start a renderer-facing indexed-surface seam, or decode raw VSWAP wall page metadata for the raycaster path. Keep headless tests comparing metadata/hashes before adding SDL3 presentation.
