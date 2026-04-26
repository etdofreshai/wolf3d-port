# Parallel Wave Note: 20260425-190622 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed requested range `36dc686b7b6a8f157f3b602a7364f41defb4ffda..HEAD`; this isolated worktree started at `36dc686`, so there were zero commits in range and no corrective review action was needed.

Also checked the latest parallel notes and `state/autopilot.md`. The latest recorded other-model work (`zai/glm-5.1@053c940`) was already included in this worktree's starting merge commit, so there was no new prior-model patch to repair before continuing.

## Work Done

- Added `wl_sample_playback_position` plus PC speaker and AdLib playback-position descriptors.
- `wl_describe_pc_speaker_playback_position()` and `wl_describe_adlib_playback_position()` map an absolute sample index to current sample/completed state without mutating playback cursors.
- The helpers reuse existing bounded sample accessors and reject out-of-range positions/truncated chunks, giving future mixer/OPL code a direct absolute-position query seam.
- Added WL6 assertions for first, last, completed-at-end, and out-of-range positions for representative PC speaker and AdLib chunks.
- Updated `docs/research/audio-assets.md` with the sample-position seam.

## Verification

```bash
cd source/modern-c-sdl3 && make test
cd source/modern-c-sdl3 && make sdl3-info test-sdl3
```

Result:

```text
asset/decompression/semantics/model/vswap/runtime-present-chase-attack-frame/audio+sod-audio tests passed for ../../../game-files/base
HAVE_SDL3=no
SDL3 not available; skipping SDL3 smoke test.
```

## Blockers

- SDL3 development files are unavailable in this isolated worktree, so SDL3 smoke coverage remains limited to the graceful skip path.
