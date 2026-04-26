# Parallel Wave Note: 20260425-205223 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed requested range `bea7d58a8710cd1ebb36c568f840ba03c40aced9..HEAD`; this isolated worktree started at `bea7d58`, so there were zero commits in the range and no corrective range-review patch was needed.

Also inspected the latest recorded other-model work, `zai/glm-5.1@9b140c6` / worker commit `2aff682`:

- The SOD boss/special tile classifications are source-backed by `WL_GAME.C::ScanInfoPlane`, reduce the optional SOD unknown histogram as documented, and are covered by headless assertions.
- No `source/original/` edits or proprietary game-file assets were present.
- No corrective issue was found, so this cycle continued with the remaining source-backed SOD static gap.

## Work Done

- Source-confirmed SOD static info tiles `72..74` from original `WL_GAME.C::ScanInfoPlane` (`SpawnStatic(x, y, tile - 23)`) and `WL_ACT1.C::statinfo`.
- Added explicit Spear static traits for types `49..52`: marble pillar/block, 25-ammo clip/bonus, truck/block, and Spear pickup/bonus.
- Added matching SOD static sprite-page mappings and map-semantics classifications without broadening path-marker or pushwall ranges.
- Updated optional SOD model assertions so representative maps and the all-map unknown-info histogram now verify zero unknown SOD info tiles.
- Updated `docs/research/sod-runtime-model-gaps.md` with the closed gap and next SOD runtime-scene follow-up.

## Verification

```bash
cd source/modern-c-sdl3
make test && make sdl3-info test-sdl3
```

Result: headless asset/decompression/semantics/model/vswap/runtime-present-chase-attack-frame/audio+sod-audio tests passed for `../../../game-files/base`; SDL3 smoke gracefully skipped with `HAVE_SDL3=no` because SDL3 development files are unavailable in this worktree.

## Blockers

None for this headless SOD static classification seam. SDL3-backed smoke remains limited to graceful skip behavior until SDL3 development files are available.
