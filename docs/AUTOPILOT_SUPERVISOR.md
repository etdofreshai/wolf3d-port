# Autopilot Supervisor

`scripts/wolf3d_autopilot_supervisor.py` is the repo-local headless Linux loop runner.

It is intentionally simple:

1. Run one OpenClaw high-reasoning autopilot cycle.
2. Let that cycle implement directly or spawn `wolf3d-*` workers.
3. Watch OpenClaw's task ledger until project-related workers finish.
4. Deliver concise progress summaries to the project chat every few minutes.
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
scripts/wolf3d_autopilot_supervisor.py --summary-interval 300
scripts/wolf3d_autopilot_supervisor.py --summary-interval 0  # disable built-in summaries
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

The supervisor can report directly to the Telegram project chat using OpenClaw delivery:

```bash
scripts/wolf3d_autopilot_supervisor.py --summary-interval 300 --summary-channel telegram --summary-target 'telegram:-5268853419'
```

This replaces the separate summary cron job when the supervisor is running.
