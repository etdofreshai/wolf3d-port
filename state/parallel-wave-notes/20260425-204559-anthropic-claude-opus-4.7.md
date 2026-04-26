# Parallel Wave Note: 20260425-204559 / anthropic-claude-opus-4.7

## Cross-Model Review

Reviewed requested range `58b2a161fead85761b39c20744393bef43712a6a..HEAD`; this worktree started at `58b2a16`, so there were zero commits in range. No corrective range-review patch was needed.

Also inspected latest parallel notes and `state/autopilot.md`. Recent fire/attack-frame work is SDL-free and test-backed. I found one small edge in the active-frame guard: an in-progress attack with depleted ammo could still fall into the no-ammo firing path and switch the current weapon to knife mid-attack. Fixed that before starting unrelated work.

## Work Done

- Made `wl_step_live_player_fire_tick()` block all fire-button attempts while `attack_frame > 0`, including no-ammo gun attempts.
- Preserved no-ammo metadata for the blocked active-frame path without consuming ammo, restarting attack frames, or switching the active weapon to knife mid-attack.
- Updated headless assertions for the active-frame/no-ammo regression.

## Verification

```bash
cd source/modern-c-sdl3
make test
```

## Blockers

None.
