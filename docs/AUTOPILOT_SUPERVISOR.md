# Autopilot Supervisor

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
```

Defaults are intentionally conservative:

- cycle thinking: `low`
- summary thinking: `low`
- fast mode: off
- model preference rotation: `openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1`
- provider usage-window override: `zai:Monthly`, while other providers default to `Week`
- extra short-window guard: `5h` must also be inside budget when the provider reports it
- clear stop files on start: on; removes stale `STOP_AUTOPILOT` and `STOP_AFTER_CURRENT_LOOP` when launching a fresh supervisor
- start update: on; sends Telegram a short debug line when the supervisor starts
- graceful stop update: on; reports loops run, elapsed time, models used, and last commit
- usage skip updates: on; report skipped models to Telegram with used vs allowed percentages
- explicit model inclusion: `--include-models` (`--models` remains an alias)
- optional model/provider exclusion after inclusion: `--exclude-models 'zai/glm-5.1'` or `--exclude-models 'zai'`
- push after cycle: on; uses normal git auth or `GH_TOKEN_ETDOFRESHAI` / `GITHUB_TOKEN` / `GH_TOKEN` from the environment
- usage guard: on; skip over-budget providers and pause only if all configured providers are over budget

Current OpenClaw CLI builds may not expose a direct `openclaw agent --model` flag. The supervisor still rotates the model preference and injects it into the cycle prompt; if a future CLI exposes `--model`, the supervisor will pass it automatically.

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

Graceful stop after the current loop finishes, including worker wait, push, completion update, and final stop update:

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
