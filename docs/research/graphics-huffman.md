# Graphics Huffman Notes

Research/implementation cycle: 2026-04-24 22:53-23:15 CDT  
Scope: `VGAHEAD`/`VGADICT`/`VGAGRAPH` header parsing, Huffman smoke decoding, `STRUCTPIC` picture-table metadata, and indexed-surface conversion for WL6 and optional SOD, without committing proprietary graphics bytes.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/ID_CA.C::GRFILEPOS`
  - reads 3-byte graphics offsets when `THREEBYTEGRSTARTS` is enabled.
  - treats `0xffffff` as sparse `-1`.
- `source/original/WOLFSRC/ID_CA.C::CAL_SetupGrFile`
  - loads `VGADICT.*` as 255 Huffman nodes.
  - loads `(NUMCHUNKS + 1)` 3-byte entries from `VGAHEAD.*`.
  - decodes `STRUCTPIC` into the picture table during setup.
- `source/original/WOLFSRC/ID_VH.H`
  - defines `pictabletype` as two 16-bit `int` fields: `width,height`.
- `source/original/WOLFSRC/ID_VH.C::VWB_DrawPic` / `LatchDrawPic`
  - index `pictable[picnum - STARTPICS]` for picture dimensions before drawing.
- `source/original/WOLFSRC/ID_VL.C::VL_MemToScreen`
  - treats graphics chunks as VGA planar memory ordered by plane, copying `width / 4` bytes per row per plane.
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
- `wl_picture_size`
- `wl_picture_table_metadata`
- `wl_decode_picture_table(...)`
- `wl_surface_format`
- `wl_indexed_surface`
- `wl_wrap_indexed_surface(...)`
- `wl_decode_planar_picture_to_indexed(...)`
- `wl_decode_planar_picture_surface(...)`

The implementation:

- parses 3-byte `VGAHEAD` entries and sparse sentinel offsets;
- parses the 255-node `VGADICT` Huffman table;
- ports the original LSB-first Huffman traversal as a pure C function;
- reads explicit-size `VGAGRAPH` chunks and decodes them into caller-provided buffers;
- decodes `STRUCTPIC` as width/height metadata for picture chunks;
- wraps decoded indexed pixels in a renderer-facing `wl_indexed_surface` descriptor with format, width, height, stride, pixel count, and pixel pointer;
- converts decoded planar picture chunks into linear 8-bit indexed surfaces suitable for a future SDL3 texture/upload boundary;
- records only sizes/counts/hashes/dimensions in tests and docs, not graphics bytes.

## WL6 committed assertions

`game-files/base/VGAHEAD.WL6` / `VGADICT.WL6` / `VGAGRAPH.WL6`:

- `VGAHEAD.WL6` entries: `150` (`149` chunks plus sentinel)
- `VGAHEAD.WL6` size: `450`
- first offsets: `0`, `354`, chunk 3 offset `9570`, final sentinel `275774`
- decoded chunk `0` (`STRUCTPIC`): compressed `354`, expanded `528`, FNV-1a `0x0a6f459a`
- `STRUCTPIC` picture table: `132` entries, width range `8..320`, height range `8..200`, total declared pixels `342464`
- representative WL6 dimensions: entry `0` `96x88`, entry `3` `320x8`, entry `84` `320x200`, entry `86` `320x200`, entry `87` `224x56`, entry `131` `224x48`
- decoded chunk `1` (`STARTFONT`): compressed `3467`, expanded `8300`, FNV-1a `0xdb48ce2b`
- decoded chunk `3`: compressed `8057`, expanded planar `8448`, planar FNV-1a `0x5c152b5c`, indexed-surface FNV-1a `0xa9c1ea92`
- decoded chunk `87` (`TITLEPIC`): compressed `45948`, expanded planar `64000`, planar FNV-1a `0x01643ebc`, indexed-surface FNV-1a `0x4b172b02`
- decoded chunk `134` (`GETPSYCHEDPIC`): compressed `5127`, expanded planar `10752`, planar FNV-1a `0xeb393cc0`, indexed-surface FNV-1a `0x46e4bd08`

## Optional SOD committed assertions

When `game-files/base/m1` is present:

- `VGAHEAD.SOD` entries: `170` (`169` chunks plus sentinel)
- `VGAHEAD.SOD` size: `510`
- first offsets: `0`, `383`, final sentinel `947979`
- decoded chunk `0` (`STRUCTPIC`): compressed `383`, expanded `588`, FNV-1a `0x43f617ea`
- `STRUCTPIC` picture table: `147` entries, width range `8..320`, height range `8..200`, total declared pixels `1105792`
- representative SOD dimensions: entry `0` `320x200`, entry `1` `104x16`, entry `84` `320x200`, entry `90` `320x80`, entry `91` `320x120`
- decoded chunk `1` (`STARTFONT`): compressed `4448`, expanded `8300`, FNV-1a `0xdb48ce2b`
- decoded chunk `3`: compressed `42248`, expanded planar `64000`, planar FNV-1a `0x3a6afac3`, indexed-surface FNV-1a `0x5e85d9c1`
- decoded chunk `90`: compressed `10561`, expanded planar `12800`, planar FNV-1a `0xa5d5a6f7`, indexed-surface FNV-1a `0xff61711d`
- decoded chunk `149`: compressed `6243`, expanded planar `10752`, planar FNV-1a `0xeb393cc0`, indexed-surface FNV-1a `0x46e4bd08`

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
asset/decompression/semantics/model/vswap/vga-surface-layer tests passed for game-files/base
```

## Cycle update: STRUCTPIC metadata

Added `wl_decode_picture_table` to interpret decoded `STRUCTPIC` chunks as original `pictabletype` width/height pairs. Tests now assert picture counts, dimension ranges, total declared pixels, and representative WL6/SOD dimensions. This creates a clean metadata bridge from decoded graphics chunks toward a renderer-facing indexed-surface seam.

## Cycle update: indexed surfaces

Added `wl_decode_planar_picture_to_indexed`, a pure C conversion from the original VGA planar chunk layout into a linear 8-bit indexed surface. Tests assert stable hashes for representative WL6 and optional SOD surfaces while keeping all pixel bytes local and uncommitted.

## Cycle update: surface descriptor layer

Added `wl_indexed_surface` and `wl_decode_planar_picture_surface`, wrapping decoded indexed pixels with the metadata a future SDL3 upload path will need: indexed-8 format, width, height, stride, pixel count, and pixel pointer. Tests assert representative WL6/SOD surface descriptors plus stable surface hashes.

## Next step

Decode raw VSWAP wall page metadata for the raycaster path, or add a tiny SDL-free blit/composite smoke test that consumes `wl_indexed_surface`. Keep headless tests comparing metadata/hashes before adding SDL3 presentation.
