# Parallel Wave Note: 20260425-114722 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed the requested range `5b656a6a0b6eefc8aeee0591e197556f4b6fe061..HEAD`; this worktree started exactly at `5b656a6`, so there were zero commits in range and no corrective review action was needed. Also checked the latest parallel notes/state before choosing work.

## Work Done

- Made `wl_default_data_dir()` discover WL6 data when tests are launched from an isolated parallel-wave worktree.
- The helper still honors `WOLF3D_DATA_DIR` first, then checks the normal main-worktree path and common source/worktree-relative paths without copying or committing proprietary game data.
- This fixes headless `make test` in `.worktrees/wave-*/<model>` checkouts while preserving the existing `game-files/base` default for the main worktree.

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

- SDL3 is not available in this worktree, so SDL3 smoke tests still exercise only the graceful skip path.
