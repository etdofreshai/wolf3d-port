# Autopilot Supervisor

For reusable/non-Wolf3D projects, see `docs/PROJECT_AUTOPILOT.md` and `scripts/project_autopilot_supervisor.py`. That generic runner accepts `--project-name`, `--project-slug`, `--project-goal`, markers, context files, protected paths, and verification focus so the same long-running autopilot pattern can be reused for projects like ETL.

`scripts/wolf3d_autopilot_supervisor.py` is the repo-local headless Linux loop runner.

It is intentionally simple:

1. Check each configured model provider against the weekly budget curve.
2. Select the next model whose provider is within budget, skipping over-budget providers.
3. Let that cycle implement directly or spawn `wolf3d-*` workers.
4. Watch OpenClaw's task ledger until project-related workers finish.
5. Commit work inside the cycle, then push the current branch after the cycle finishes.
6. Deliver a concise progress update to the project chat when a cycle/work unit finishes.
7. Start the next cycle.

Durable state remains in the repo:

- `docs/AUTOPILOT.md` - standing rules
- `state/autopilot.md` - current project state
- `state/autopilot-supervisor-model-state.json` - last selected model-rotation slot, ignored by git
- git commits - verified progress history
- `logs/autopilot-supervisor.log` - supervisor runtime log, ignored by git

## Run

```bash
scripts/wolf3d_autopilot_supervisor.py
```

Useful options:

```bash
scripts/wolf3d_autopilot_supervisor.py --max-cycles 1
scripts/wolf3d_autopilot_supervisor.py --thinking low --summary-thinking low
scripts/wolf3d_autopilot_supervisor.py --include-models 'openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1'
scripts/wolf3d_autopilot_supervisor.py --exclude-models 'zai/glm-5.1'
scripts/wolf3d_autopilot_supervisor.py --exclude-models 'zai'  # exclude a whole provider prefix
scripts/wolf3d_autopilot_supervisor.py --review-previous-steps --review-fallback-commit-count 3
scripts/wolf3d_autopilot_supervisor.py --usage-provider-windows 'zai:Monthly,openai-codex:Week'
scripts/wolf3d_autopilot_supervisor.py --usage-extra-windows '5h'
scripts/wolf3d_autopilot_supervisor.py --usage-skip-updates
scripts/wolf3d_autopilot_supervisor.py --no-fast-mode
scripts/wolf3d_autopilot_supervisor.py --worker-poll 30 --max-worker-wait 7200
scripts/wolf3d_autopilot_supervisor.py --push-after-cycle
scripts/wolf3d_autopilot_supervisor.py --no-push-after-cycle
scripts/wolf3d_autopilot_supervisor.py --completion-summary
scripts/wolf3d_autopilot_supervisor.py --no-completion-summary
scripts/wolf3d_autopilot_supervisor.py --start-update
scripts/wolf3d_autopilot_supervisor.py --stop-update
scripts/wolf3d_autopilot_supervisor.py --clear-stop-files-on-start
scripts/wolf3d_autopilot_supervisor.py --summary-interval 300  # optional periodic summaries while waiting
scripts/wolf3d_parallel_wave.py --max-waves 1 --parallel-max-models 3
```

Defaults are intentionally conservative:

- cycle thinking: `low`
- summary thinking: `low`
- fast mode: off
- model preference rotation: `openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1`
- provider usage-window override: `zai:Monthly`, while other providers default to `Week`
- extra short-window guard: `5h` must also be inside budget when the provider reports it
- cross-model review: on when multiple models are configured; inspect commits since the selected model last completed work
- clear stop files on start: on; removes stale `STOP_AUTOPILOT` and `STOP_AFTER_CURRENT_LOOP` when launching a fresh supervisor
- start update: on; sends Telegram a short debug line when the supervisor starts
- graceful stop update: on; reports loops run, elapsed time, models used, and last commit
- usage skip updates: on; report skipped models to Telegram with used vs allowed percentages
- explicit model inclusion: `--include-models` (`--models` remains an alias)
- optional model/provider exclusion after inclusion: `--exclude-models 'zai/glm-5.1'` or `--exclude-models 'zai'`
- push after cycle: on; uses normal git auth or `GH_TOKEN_ETDOFRESHAI` / `GITHUB_TOKEN` / `GH_TOKEN` from the environment
- usage guard: on; skip over-budget providers and pause only if all configured providers are over budget

Current OpenClaw CLI builds may not expose a direct `openclaw agent --model` flag. The supervisor still rotates the model preference and injects it into the cycle prompt; if a future CLI exposes `--model`, the supervisor will pass it automatically.

## Parallel wave mode

`scripts/wolf3d_parallel_wave.py` is the opt-in parallel “beads” runner. It launches at most one worker per eligible model, each in an isolated git worktree and branch under ignored `.worktrees/`, waits for all workers to finish, then merges successful branches back into `main` one at a time.

One wave:

```bash
scripts/wolf3d_parallel_wave.py --max-waves 1 --parallel-max-models 3
```

Behavior:

- checks the same provider usage windows before selecting models
- creates one worktree/branch per selected model
- gives every worker the same cross-model review instructions before new work
- tells workers to record parallel-wave notes under `state/parallel-wave-notes/` instead of editing merge-hot `state/autopilot.md`
- runs workers concurrently with separate OpenClaw sessions
- records worker JSON logs under ignored `logs/`
- merges successful branches sequentially
- runs `make test` after merge
- pushes if verification passes
- reports auto-incrementing wave number, per-model elapsed time, brief per-model work summary, merged commits, issues/conflicts, verification, push status, and final head to Telegram
- when a merge conflicts, asks resolver agents to repair the conflict and run verification before giving up; defaults to up to 3 resolver attempts

If a branch conflicts and the resolver attempts cannot safely fix it, or if verification fails, the wave reports the issue and stops instead of blindly continuing. This is deliberately safer than letting multiple models edit the same checkout.

## Cross-model review phase

When more than one model is configured, each cycle includes a short review phase before new implementation work. The selected model inspects the latest commits, recent cycle logs, and `state/autopilot.md`, looking especially at work likely produced under a different model preference.

Default behavior:

- `--review-previous-steps` enabled
- track the last completed commit per model in ignored runtime state
- when a model runs again, review commits since that same model last completed work
- if the selected model has no recorded prior completion yet, inspect the last `3` commits via `--review-fallback-commit-count 3`
- if no other model work is present in the range, do only a brief sanity check
- fix concrete issues from reviewed work before starting unrelated new features
- if recent work looks sound, add a terse review note to `state/autopilot.md` and continue with the next best porting step
- keep review tied to code/tests/docs; avoid long abstract critique

Disable with:

```bash
scripts/wolf3d_autopilot_supervisor.py --no-review-previous-steps
```

## Usage budget guard

The supervisor checks `openclaw status --usage --json` before each cycle. It infers each model's provider from the prefix before `/` in `--models` (`openai-codex/gpt-5.5` → `openai-codex`, `anthropic/claude-opus-4.7` → `anthropic`, `zai/glm-5.1` → `zai`) and compares that provider's `usedPercent` to the configured window's budget curve.

Default policy:

- default to the `Week` usage window unless a provider override exists
- use `zai:Monthly` by default because Z.ai reports a monthly quota window
- infer the budget start from the window label: `Monthly` → `resetAt - 30 days`, `Week` → `resetAt - 7 days`, `5h` → `resetAt - 5 hours`
- check both the provider's long window and any `--usage-extra-windows` such as `5h`
- allow usage up to `elapsed_window_percent + 5%` slack for each checked window
- allow at least `3%` early in the window
- if one provider is ahead of schedule, skip that model, send a compact Telegram update with used vs allowed percentages, and try the next configured provider
- if all configured providers are ahead of schedule, report the skipped models, sleep until the earliest provider should be back inside budget, then re-check

Useful options:

```bash
scripts/wolf3d_autopilot_supervisor.py --include-models 'openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1'
scripts/wolf3d_autopilot_supervisor.py --exclude-models 'anthropic/claude-opus-4.7'
scripts/wolf3d_autopilot_supervisor.py --exclude-models 'zai'
scripts/wolf3d_autopilot_supervisor.py --usage-window Week --usage-provider-windows 'zai:Monthly'
scripts/wolf3d_autopilot_supervisor.py --usage-extra-windows '5h'
scripts/wolf3d_autopilot_supervisor.py --no-usage-skip-updates
scripts/wolf3d_autopilot_supervisor.py --usage-budget-start 2026-04-22T12:33:16Z
scripts/wolf3d_autopilot_supervisor.py --usage-budget-reset 2026-04-29T12:33:16Z
scripts/wolf3d_autopilot_supervisor.py --usage-slack-percent 10
scripts/wolf3d_autopilot_supervisor.py --usage-check-interval 1800
scripts/wolf3d_autopilot_supervisor.py --no-usage-guard
```

This is deliberately simple: near the start of each long or short window, each provider spends slowly; near the end, each provider can use the full 100%. If Codex is over budget but Anthropic or Z.ai is still under budget on both long and short windows, the supervisor should continue on the available provider instead of pausing the whole autopilot.

## Stop

By default, launching a fresh supervisor clears stale stop files first. Disable that with `--no-clear-stop-files-on-start` if you intentionally want existing stop files to remain.

Immediate stop before another cycle starts:

```bash
touch state/STOP_AUTOPILOT
```

Graceful stop after the current loop finishes, including worker wait, push, completion update, final stop update, and cleanup of `STOP_AFTER_CURRENT_LOOP`:

```bash
touch state/STOP_AFTER_CURRENT_LOOP
```

Or use the supervisor helper command from the repo:

```bash
scripts/wolf3d_autopilot_supervisor.py --stop-after-current-loop
```

If needed, kill the PID in:

```bash
cat state/autopilot-supervisor.pid
```

## Why this instead of only cron?

Cron is time-based. The supervisor is completion-based: it waits for spawned workers to finish, then immediately starts the next adaptive cycle if at least one configured provider's usage budget allows. That better matches the desired “chain work all night” behavior without accidentally burning a whole weekly token budget.

## Push after cycle

The cycle prompt tells the agent to commit useful changes. After the cycle and any project workers finish, the supervisor runs `git push origin <current-branch>` before sending the chat update.

If a token is available in `GH_TOKEN_ETDOFRESHAI`, `GITHUB_TOKEN`, or `GH_TOKEN`, the supervisor uses a temporary `GIT_ASKPASS` helper. Otherwise it falls back to normal git credentials/SSH. The temporary helper is ignored by git and removed after use.

Disable automatic pushes with:

```bash
scripts/wolf3d_autopilot_supervisor.py --no-push-after-cycle
```

## Chat updates

The supervisor reports directly to the Telegram project chat using OpenClaw delivery when it starts, when models are skipped for usage, after each completed cycle/work unit, and after a graceful stop. Updates are intentionally short. Completion updates should include model used, provider usage/window when available, elapsed time when available, commit/push status, verification, and terse work completed:

```bash
scripts/wolf3d_autopilot_supervisor.py --completion-summary --summary-channel telegram --summary-target 'telegram:-5268853419'
```

Optional periodic updates while waiting for long workers can be enabled with `--summary-interval 300`, but the preferred mode is completion-based reporting. This replaces the separate summary cron job when the supervisor is running.
