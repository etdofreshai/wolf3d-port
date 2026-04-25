# Autopilot Supervisor

`scripts/wolf3d_autopilot_supervisor.py` is the repo-local headless Linux loop runner.

It is intentionally simple:

1. Run one OpenClaw high-reasoning autopilot cycle.
2. Let that cycle implement directly or spawn `wolf3d-*` workers.
3. Watch OpenClaw's task ledger until project-related workers finish.
4. Deliver a concise progress summary to the project chat when a cycle/work unit finishes.
5. Start the next cycle.

Durable state remains in the repo:

- `docs/AUTOPILOT.md` - standing rules
- `state/autopilot.md` - current project state
- git commits - verified progress history
- `logs/autopilot-supervisor.log` - supervisor runtime log, ignored by git

## Run

```bash
scripts/wolf3d_autopilot_supervisor.py
```

Useful options:

```bash
scripts/wolf3d_autopilot_supervisor.py --max-cycles 1
scripts/wolf3d_autopilot_supervisor.py --worker-poll 30 --max-worker-wait 7200
scripts/wolf3d_autopilot_supervisor.py --completion-summary
scripts/wolf3d_autopilot_supervisor.py --no-completion-summary
scripts/wolf3d_autopilot_supervisor.py --summary-interval 300  # optional periodic summaries while waiting
```

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

Cron is time-based. The supervisor is completion-based: it waits for spawned workers to finish, then immediately starts the next adaptive cycle. That better matches the desired “chain work all night” behavior.

## Chat summaries

The supervisor reports directly to the Telegram project chat using OpenClaw delivery after each completed cycle/work unit by default:

```bash
scripts/wolf3d_autopilot_supervisor.py --completion-summary --summary-channel telegram --summary-target 'telegram:-5268853419'
```

Optional periodic summaries while waiting for long workers can be enabled with `--summary-interval 300`, but the preferred mode is completion-based reporting. This replaces the separate summary cron job when the supervisor is running.
