# Generic Project Autopilot Supervisor

`scripts/project_autopilot_supervisor.py` is a reusable OpenClaw loop runner for long-running repos.

It generalizes the Wolf3D autopilot pattern:

1. Select an available model from a rotation.
2. Ask OpenClaw to run one adaptive project cycle.
3. Wait for project-matching background tasks/subagents to finish.
4. Optionally push the current branch.
5. Deliver a short progress update.
6. Repeat until stopped, budget-blocked, or `--max-cycles` is reached.

Durable state lives in repo files under `state/`, `logs/`, docs, and git commits. The script should not rely on hidden in-process memory for project direction.

## Wolf3D-compatible run

```bash
scripts/project_autopilot_supervisor.py
```

The defaults preserve the Wolf3D settings:

- `--project-name Wolf3D`
- `--project-slug wolf3d`
- markers: `wolf3d,wolfenstein 3d,wolfenstein`
- context files: `docs/AUTOPILOT.md,state/autopilot.md`
- protected paths: `source/original/,game-files/`
- extra instruction: do not commit proprietary game assets

## One cycle, no push/update

Useful for testing prompt shape safely:

```bash
scripts/project_autopilot_supervisor.py \
  --max-cycles 1 \
  --no-push-after-cycle \
  --no-start-update \
  --no-completion-summary
```

## New project example: ETL

```bash
scripts/project_autopilot_supervisor.py \
  --repo /home/node/.openclaw/tmp/etl-scripting-language \
  --project-name ETL \
  --project-slug etl \
  --project-goal 'minimal LLM-oriented self-hosting scripting language and compiler' \
  --project-markers 'etl,etl scripting language,self-hosting compiler' \
  --context-files 'docs/AUTOPILOT.md,state/autopilot.md,docs/SPEC.md' \
  --protected-paths '' \
  --verification-focus 'Run parser/compiler tests and any bootstrap smoke test.' \
  --extra-instruction 'Keep ETL v0 minimal; prefer small verified compiler increments.'
```

## Stop files

The supervisor honors both project-specific and generic stop files:

```bash
touch state/STOP_ETL_AUTOPILOT
touch state/STOP_ETL_AFTER_CURRENT_LOOP
```

For backward compatibility it also honors:

```bash
touch state/STOP_AUTOPILOT
touch state/STOP_AFTER_CURRENT_LOOP
```

Or ask it to request graceful stop:

```bash
scripts/project_autopilot_supervisor.py --project-slug etl --stop-after-current-loop
```

## Important options

```bash
--models 'openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1'
--exclude-models 'zai'                 # exclude provider or exact model
--thinking low
--summary-thinking low
--usage-guard / --no-usage-guard
--usage-window Week
--summary-target telegram:<chat-id>
--summary-interval 300
--push-after-cycle / --no-push-after-cycle
```

## Notes

- Project matching uses `--project-markers` plus the repo path to decide which OpenClaw tasks belong to this project.
- Labels for spawned workers should be prefixed with `--project-slug`, e.g. `etl-parser-worker`.
- The script is intentionally generic; project-specific strategy belongs in the repo's `docs/AUTOPILOT.md` and `state/autopilot.md`.
- `scripts/wolf3d_autopilot_supervisor.py` remains available as the richer Wolf3D-specific runner. This generic script is the version to copy or parameterize for new repos.
