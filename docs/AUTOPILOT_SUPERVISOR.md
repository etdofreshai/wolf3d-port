# Autopilot Supervisor

`scripts/wolf3d_autopilot_supervisor.py` is the repo-local headless Linux loop runner.

It is intentionally simple:

1. Check provider usage against the configured weekly budget curve.
2. Run one OpenClaw autopilot cycle with configurable thinking/model preference.
3. Let that cycle implement directly or spawn `wolf3d-*` workers.
4. Watch OpenClaw's task ledger until project-related workers finish.
5. Deliver a concise progress summary to the project chat when a cycle/work unit finishes.
6. Start the next cycle.

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
scripts/wolf3d_autopilot_supervisor.py --models 'openai-codex/gpt-5.5,anthropic/claude-opus-4.7'
scripts/wolf3d_autopilot_supervisor.py --no-fast-mode
scripts/wolf3d_autopilot_supervisor.py --worker-poll 30 --max-worker-wait 7200
scripts/wolf3d_autopilot_supervisor.py --completion-summary
scripts/wolf3d_autopilot_supervisor.py --no-completion-summary
scripts/wolf3d_autopilot_supervisor.py --summary-interval 300  # optional periodic summaries while waiting
```

Defaults are intentionally conservative:

- cycle thinking: `low`
- summary thinking: `low`
- fast mode: off
- model preference rotation: `openai-codex/gpt-5.5,anthropic/claude-opus-4.7`
- usage guard: on

Current OpenClaw CLI builds may not expose a direct `openclaw agent --model` flag. The supervisor still rotates the model preference and injects it into the cycle prompt; if a future CLI exposes `--model`, the supervisor will pass it automatically.

## Usage budget guard

The supervisor checks `openclaw status --usage --json` before each cycle. By default it watches the `openai-codex` provider's `Week` window and compares the reported `usedPercent` to a linear weekly budget curve.

Default policy:

- infer the budget start as `resetAt - 7 days`
- allow usage up to `elapsed_week_percent + 5%` slack
- allow at least `3%` early in the window
- if usage is ahead of schedule, sleep and re-check periodically instead of starting another cycle

Useful options:

```bash
scripts/wolf3d_autopilot_supervisor.py --usage-provider openai-codex --usage-window Week
scripts/wolf3d_autopilot_supervisor.py --usage-budget-start 2026-04-22T12:33:16Z
scripts/wolf3d_autopilot_supervisor.py --usage-budget-reset 2026-04-29T12:33:16Z
scripts/wolf3d_autopilot_supervisor.py --usage-slack-percent 10
scripts/wolf3d_autopilot_supervisor.py --usage-check-interval 1800
scripts/wolf3d_autopilot_supervisor.py --no-usage-guard
```

This is deliberately simple: near the start of the week, the autopilot spends slowly; near the end, it can use the full 100%.

## Stop

Create this file:

```bash
touch state/STOP_AUTOPILOT
```

Or kill the PID in:

```bash
cat state/autopilot-supervisor.pid
```

## Why this instead of only cron?

Cron is time-based. The supervisor is completion-based: it waits for spawned workers to finish, then immediately starts the next adaptive cycle if usage budget allows. That better matches the desired “chain work all night” behavior without accidentally burning a whole weekly token budget.

## Chat summaries

The supervisor reports directly to the Telegram project chat using OpenClaw delivery after each completed cycle/work unit by default:

```bash
scripts/wolf3d_autopilot_supervisor.py --completion-summary --summary-channel telegram --summary-target 'telegram:-5268853419'
```

Optional periodic summaries while waiting for long workers can be enabled with `--summary-interval 300`, but the preferred mode is completion-based reporting. This replaces the separate summary cron job when the supervisor is running.
