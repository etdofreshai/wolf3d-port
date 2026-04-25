# Modern C + SDL3 Port

Target implementation for the Wolfenstein 3D modern C port using SDL3.

Goals:

- Stay close to original Wolfenstein 3D behavior and structure where practical.
- Replace DOS/platform-specific services with SDL3-backed equivalents.
- Keep game data proprietary assets outside git in `game-files/`.
- Use tests as the behavioral bridge from original source to this implementation.

## Current headless verification

The first runnable milestone is a pure C asset/decompression/semantics/model harness. It has no SDL dependency yet and verifies local WL6 data discovery, `MAPHEAD`, `GAMEMAPS`, `VGAHEAD`/`VGADICT`/`VGAGRAPH` Huffman graphics smoke decoding, `STRUCTPIC` picture-table metadata, planar-to-indexed picture surface conversion with indexed-surface descriptors, upload metadata, RGBA expansion, and SDL-free blitting, full `VSWAP` chunk-directory parsing, bounded chunk reads, wall-page metadata/surface conversion, wall texture-column sampling, fixed-height wall strip scaling, tiny wall-strip viewport composition, map-derived wall-hit descriptors, cardinal/fixed-point/DDA/projected ray stepping, view batches, and camera ray tables, and tiny view rendering, sprite shape metadata, sprite post-command metadata, Carmack/RLEW map-plane decompression, first-pass map semantic classification, and a minimal `SetupGameLevel`-style runtime model.

Run from this directory:

```bash
make test
```

By default the test uses `game-files/base` relative to the repo root. Override with:

```bash
WOLF3D_DATA_DIR=/path/to/WL6/data make test
```

The test only reads local data files and commits metadata/hash/semantic/model/chunk-count expectations, not proprietary game bytes.
