# Autopilot Supervisor

`scripts/wolf3d_autopilot_supervisor.py` is the repo-local headless Linux loop runner.

It is intentionally simple:

1. Run one OpenClaw high-reasoning autopilot cycle.
2. Let that cycle implement directly or spawn `wolf3d-*` workers.
3. Watch OpenClaw's task ledger until project-related workers finish.
4. Start the next cycle.

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
