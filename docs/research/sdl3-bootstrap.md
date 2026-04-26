# SDL3 Bootstrap Boundary

The port keeps deterministic headless C tests as the default gate, but SDL3 presentation work needs an optional local dependency path on machines that do not have SDL3 development files installed.

## Local dependency layout

`scripts/bootstrap_sdl3.sh` clones and builds SDL3 into ignored repo-local paths:

- `.deps/SDL-src/` - SDL source checkout
- `.deps/SDL-build/` - CMake build tree
- `.deps/SDL3/` - install prefix with `sdl3.pc`

These paths are ignored and should not be committed.

## Use

```bash
scripts/bootstrap_sdl3.sh
cd source/modern-c-sdl3
make test-sdl3
```

The script requires `git`, `cmake`, a C compiler, and normal SDL3 build prerequisites. It does not install system packages and does not modify `/usr`.

## Headless smoke seam

`source/modern-c-sdl3/tests/test_sdl3_smoke.c` initializes SDL video, creates a hidden 64x64 window, obtains the window surface, fills it, and updates it. The make target runs it with `SDL_VIDEODRIVER=dummy` so future presentation checks can remain headless.

If SDL3 is unavailable, `make test-sdl3` prints a skip message and exits successfully. The normal `make test` gate remains SDL-free.

## Headless/Linux dependency note

On this Debian 12 container, SDL3 could be built without system SDL development packages by using repo-local CMake/Ninja tools and configuring SDL with desktop/audio backends disabled plus `SDL_UNIX_CONSOLE_BUILD=ON`. The resulting enabled presentation backends include `dummy` and `offscreen`, which are enough for the first headless smoke tests.

Because this repo path contains spaces, the Makefile prefers relative `.deps/SDL3` include/lib paths when the repo-local `sdl3.pc` exists. That avoids unquoted absolute paths emitted by `pkg-config` from being split by the shell.

## SDL3 present smoke seam

`tests/test_sdl3_present.c` adds the first SDL-backed presentation check above the basic initialization smoke test. It builds a deterministic 4x4 indexed frame, expands the palette entries to RGBA with the same 6-bit-to-8-bit shape used by the headless upload tests, verifies the source RGBA hash, creates a hidden SDL3 window with the dummy video driver, blits the RGBA surface to the window surface, and updates the window.

This is intentionally tiny: it proves the repo-local SDL3 install can accept renderer-facing RGBA pixels in a headless presentation path before larger Wolfenstein scene screenshots are introduced.

## SDL3 Wolf wall present seam

`tests/test_sdl3_present.c` now feeds actual local WL6 VSWAP data into the SDL3 presentation smoke path. It reads `VSWAP.WL6`, decodes wall page 0 to an indexed `wl_indexed_surface`, describes the frame through `wl_describe_present_frame`, expands it to RGBA via `wl_expand_indexed_surface_to_rgba`, and blits that frame to a hidden SDL3 window surface under the dummy video driver.

The committed assertions remain metadata-only: wall indexed hash `0x8fe4d8ff` and RGBA hash `0x71d4b5b6`. No decoded proprietary pixels are committed.


## SDL3 Wolf wall screenshot artifact

The SDL3 present test now saves the headless wall-frame surface to ignored `source/modern-c-sdl3/build/wolf-wall-present.bmp` after blitting/updating the hidden dummy-driver window. The test asserts the artifact exists and is non-empty; decoded proprietary pixels remain generated-only under `build/` and are not committed.


## SDL3 palette-shifted screenshot artifact

The Wolf wall present smoke now also routes a red-shifted present descriptor through the same SDL dummy-window path and saves ignored `source/modern-c-sdl3/build/wolf-wall-present-red.bmp`. Assertions pin indexed hash `0x8fe4d8ff`, red palette hash `0xd0d5c585`, red RGBA hash `0x1dcaf8c4`, and non-empty screenshot files.


## SDL3 screenshot artifact hashes

The SDL3 present smoke now hashes the generated BMP artifacts themselves, not just the source indexed/RGBA buffers. The base wall screenshot is pinned at size `16522` and hash `0xb49b4cbf`; the red-shifted wall screenshot is pinned at size `16522` and hash `0xaa1c75c5`. This keeps screenshot verification deterministic while artifacts remain ignored.


## SDL3 wall atlas screenshot seam

The SDL3 present smoke now routes a fuller two-wall WL6 atlas through the screenshot seam. It decodes wall pages 0 and 1, composes a `128x64` indexed surface, verifies wall-1 hash `0xcc7509fd`, atlas indexed hash `0x223d2caf`, atlas RGBA hash `0x3a8ae4e9`, and saves ignored `build/wolf-wall-atlas-present.bmp` with size/hash `32906` / `0xaf70162c`.


## SDL3 sprite screenshot seam

The SDL3 present smoke now routes sprite-bearing output through the screenshot seam. It decodes VSWAP sprite chunk `106`, verifies sprite hash `0x918ed728`, renders it onto a `128x64` indexed canvas with hash `0xb7087e58`, expands to RGBA hash `0x6159f78f`, and saves ignored `build/wolf-sprite-present.bmp` with size/hash `32906` / `0xbaeda862`.

## Pitched RGBA presentation upload seam

The SDL-free presentation helpers now include `wl_expand_present_frame_to_rgba_pitched()`, which expands a `wl_present_frame_descriptor` into caller-provided RGBA rows with an explicit destination pitch. This keeps the deterministic indexed/palette path usable for SDL surfaces or textures that expose row padding instead of requiring tightly packed `width * 4` rows. The existing tight `wl_expand_present_frame_to_rgba()` path delegates through the pitched helper, and headless tests verify padded rows preserve their sentinel bytes while visible pixels match the tight upload.

## Pitched RGBA upload descriptor seam

`wl_describe_present_frame_rgba_upload_pitched()` now describes caller-owned RGBA buffers with explicit row pitch after validating the same present-frame layout constraints used by expansion. The pitched expansion helper delegates descriptor creation through it, so future SDL3 surface/texture handoff paths can preflight padded rows and upload metadata without duplicating pitch/size checks. The descriptor and expansion paths now require the caller buffer to cover the full `pitch * height` span reported in `pixel_bytes`, even when no output descriptor is requested. That keeps the pitched-RGBA contract consistent for SDL surfaces/textures whose row pitch includes padding.

`wl_present_frame_rgba_padding()` reports the per-row and total padding implied by a caller-provided RGBA pitch after the same present-frame layout validation. This gives future SDL3 surface/texture paths a tiny SDL-free preflight for distinguishing visible bytes from row padding before deciding whether to preserve, clear, or upload padded spans.

## Padded RGBA clear helper

`wl_clear_present_frame_rgba_padding()` now clears only the per-row padding bytes of a pitched RGBA present-frame buffer after the indexed pixels have been expanded. The helper reuses present-frame layout validation, preserves visible RGBA pixels, rejects undersized padded spans, and no-ops for tight rows. This keeps future SDL3 surface/texture upload paths deterministic when callers need padding bytes initialized before screenshot/hash/presentation handoff.

`wl_present_frame_rgba_required_size()` centralizes the full `pitch * height` span check for pitched RGBA present-frame buffers. Padded upload descriptors and padding-clear validation now share that preflight, while tests cover tight, padded, narrow-pitch, null-output, and oversized-width rejection cases.

## Cycle update: padded RGBA expansion helper

`wl_expand_present_frame_to_rgba_pitched_clear_padding()` now combines indexed present-frame RGBA expansion with deterministic per-row padding initialization. This keeps future SDL3 surface/texture handoff code from duplicating the expand-then-clear sequence when using padded pitches while preserving the same pitch/size validation used by the lower-level helpers.

## SDL3 live gameplay scene screenshot seam

The SDL3 present smoke now routes a small runtime/gameplay-rendered scene through the headless presentation path. The test builds a mutable `wl_game_model` with a closed runtime door and a visible sprite, renders it with `wl_render_runtime_door_camera_scene_view`, describes the resulting indexed scene as a present frame, expands it to RGBA, and saves ignored `build/wolf-live-scene-present.bmp` under the dummy video driver. Assertions pin the live scene indexed hash `0x8463d905`, RGBA hash `0xd6cf31ac`, and BMP size/hash `41098` / `0x2ac02cbd` without committing decoded proprietary pixels.
