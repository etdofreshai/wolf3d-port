# Parallel Wave Note: 20260425-185741 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed requested range `30f4e4a689143a4b1907dbb8d3a3a0f8878b962c..HEAD`; this isolated worktree started at `30f4e4a`, so there were zero commits in range and no corrective prior-model review action was needed. I also checked the latest parallel notes and `state/autopilot.md` before choosing work.

## Work Done

- Added `wl_sample_playback_window` plus PC speaker and AdLib playback-window descriptors.
- `wl_describe_pc_speaker_playback_window()` and `wl_describe_adlib_playback_window()` summarize bounded sample windows from a start sample and sample budget, reporting availability, emitted count, next sample, first/last emitted sample, and completion.
- The helpers reuse the existing bounded sample accessors so future mixer/OPL playback code does not duplicate raw chunk offset math.
- Added WL6 assertions for mid-sound windows, end-of-sound completion windows, and invalid start positions for both PC speaker and AdLib chunks.
- Updated `docs/research/audio-assets.md` with the new sample-window seam.

## Verification

```bash
cd source/modern-c-sdl3
make test
make sdl3-info test-sdl3
```

Result: headless asset/decompression/runtime/audio tests passed for `../../../game-files/base`; SDL3 smoke gracefully skipped because `HAVE_SDL3=no` in this worktree.

## Blockers

- SDL3 development files are unavailable in this isolated worktree, so SDL3 smoke coverage remains limited to the graceful skip path.
