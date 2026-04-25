# Modern C + SDL3 Port

Target implementation for the Wolfenstein 3D modern C port using SDL3.

Goals:

- Stay close to original Wolfenstein 3D behavior and structure where practical.
- Replace DOS/platform-specific services with SDL3-backed equivalents.
- Keep game data proprietary assets outside git in `game-files/`.
- Use tests as the behavioral bridge from original source to this implementation.

## Current headless verification

The first runnable milestone is a pure C asset/decompression/semantics/model harness. It has no SDL dependency yet and verifies local WL6 data discovery, `MAPHEAD`, `GAMEMAPS`, `VGAHEAD`/`VGADICT`/`VGAGRAPH` Huffman graphics smoke decoding, `STRUCTPIC` picture-table metadata, planar-to-indexed picture surface conversion with indexed-surface descriptors, upload metadata, palette interpolation/fade/shift metadata, gameplay palette-shift state, and palette-shifted upload selection, RGBA expansion, and SDL-free blitting, full `VSWAP` chunk-directory parsing, bounded chunk reads, wall-page metadata/surface conversion, wall texture-column sampling, fixed-height wall strip scaling, tiny wall-strip viewport composition, map-derived wall-hit descriptors, cardinal/fixed-point/DDA/projected ray stepping, view batches, and camera ray tables, and tiny view rendering, sprite shape metadata, sprite post-command metadata, sprite indexed surfaces, scaled sprite rendering, world-space sprite projection/ordering, combined wall+sprite scene rendering, Carmack/RLEW map-plane decompression, first-pass map semantic classification, a minimal `SetupGameLevel`-style runtime model, and runtime sprite-reference selection, sprite surface-cache decoding, and cached runtime-ref sprites feeding combined wall+sprite scene rendering across representative visible refs/camera angles, and multi-map runtime actor coverage for guards, SS, officers, dogs, mutants, bosses, and ghosts, plus player damage/bonus pickup/score/ammo/weapon/key event seams and runtime static pickup/removal hooks, a TransformTile-style visible pickup probe, and a small player motion/collision tick, and use-button door/elevator/pushwall dispatch metadata, and deterministic door open/close progression, and pushwall start/progression metadata, and a live runtime solid-plane raycast/render bridge.

Run from this directory:

```bash
make test
```

By default the test uses `game-files/base` relative to the repo root. Override with:

```bash
WOLF3D_DATA_DIR=/path/to/WL6/data make test
```

The test only reads local data files and commits metadata/hash/semantic/model/chunk-count expectations, not proprietary game bytes.


## Runtime wall-view bridge

`wl_build_runtime_solid_plane` converts mutable model `tilemap` state into the existing solid wall-plane raycaster, and `wl_render_runtime_camera_wall_view` feeds that bridge directly into the headless wall-view renderer. Current tests verify that a closed door blocks and renders as a placeholder solid, then clearing that same door lets the camera rays continue to a later wall with a different deterministic canvas hash. Moving pushwall solid markers are also preserved for ray hits.
