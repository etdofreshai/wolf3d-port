# Parallel Wave Note: 20260425-190147 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed the requested range `1faf8d2eee6e1eb2c083d536c75d0a78b670d82f..HEAD`: this isolated worktree started at `1faf8d2`, so there were zero commits in that range and no corrective range review action was needed.

Also reviewed the latest recorded other-model work `zai/glm-5.1@dca8634` / `77a4262`:

- The PC speaker playback cursor seam is bounded by the existing chunk length/sample count validation, reuses the checked sample accessor for current-sample reads, and is covered by WL6 assertions for active, clamped-complete, completed, invalid, and truncated states.
- The code/docs/tests avoid `source/original/` and proprietary game-file assets.
- No corrective issue was found, so this cycle extended the analogous AdLib sample scheduling seam.

## Work Done

- Added `wl_adlib_playback_cursor` and `wl_advance_adlib_playback_cursor()` for deterministic frame-to-frame AdLib sound sample progression.
- Factored the common sample cursor advancement logic so PC speaker and AdLib playback cursors share clamping/completion/current-sample behavior while keeping format-specific chunk bounds.
- Added WL6 assertions for zero-delta current sample, last-sample progression, clamped completion, completed no-op, invalid completed delta, and truncated-chunk rejection on representative AdLib chunk 87.
- Updated `docs/research/audio-assets.md` with the AdLib playback cursor seam.

## Verification

```bash
cd source/modern-c-sdl3
make test
make sdl3-info test-sdl3
```

Result:

```text
asset/decompression/semantics/model/vswap/runtime-present-chase-attack-frame/audio+sod-audio tests passed for ../../../game-files/base
HAVE_SDL3=no
SDL3 not available; skipping SDL3 smoke test.
```

## Blockers

- SDL3 development files are unavailable in this isolated worktree, so SDL3 smoke coverage remains limited to graceful skip behavior.
