# Parallel Wave Note: 20260425-185442 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed requested range `489077f34d5cae14b5d02687fd10329bfbe8470c..HEAD`. The concrete prior model work in that range was `zai/glm-5.1` audio accessor work, especially:

- `4ab9330` Add bounded AdLib sample accessor
- `99eaed4` Add bounded PC speaker sample accessor

I inspected the patches, latest parallel notes, and `state/autopilot.md`. The prior work looks sound: accessors share existing bounds logic, avoid proprietary asset commits, and have headless tests/docs. No corrective fix was needed.

## Work Done

- Added `wl_get_adlib_instrument_byte()` as a bounded accessor for the 16-byte AdLib instrument block.
- Reused the accessor inside `wl_describe_adlib_sound()` so metadata and future OPL setup code share the same offset/bounds path.
- Added WL6 assertions for first/last/out-of-range instrument byte reads and truncated-chunk rejection.
- Updated `docs/research/audio-assets.md` with the instrument accessor seam.

## Verification

```bash
cd source/modern-c-sdl3
make test && make sdl3-info test-sdl3
```

Result: headless asset/decompression/runtime/audio tests passed; SDL3 smoke gracefully skipped because `HAVE_SDL3=no` in this worktree.

## Blockers

- None expected beyond existing SDL3 dev-file availability; SDL3 smoke may gracefully skip when unavailable.
