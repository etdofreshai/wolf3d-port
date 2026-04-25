# Wolfenstein 3D Port Autopilot

This project is intended to run as an adaptive long-running OpenClaw/TaskFlow-driven effort.

## Owner Intent

Port Wolfenstein 3D into modern, understandable implementations across multiple languages/runtimes, starting from the original id Software source and legally supplied local game data.

The owner wants to specify intent with minimal prompting. The autopilot should make reasonable technical decisions independently and keep moving toward the goal.

## Prime Directive

Make the best next move toward a working, verified port. Do not wait for perfect instructions. Prefer useful progress over asking for clarification.

## Ask/Escalate Only When Necessary

Do **not** ask the owner for routine decisions. Take the best guess and record the rationale.

Escalate only for:

- Legal/licensing uncertainty beyond normal local-use assumptions
- Destructive operations or irreversible data loss
- External/public publishing, messaging, or uploads
- A true product-direction fork where both options are expensive and mutually exclusive
- Missing credentials/assets that cannot be inferred or worked around
- Repeated verification failure after several materially different attempts

Everything else should be handled autonomously.

## Loop Shape

Each autopilot cycle should:

1. **Observe**
   - Inspect repo state, prior notes, current build/test status, and active blockers.
2. **Evaluate**
   - Choose the highest-leverage next step based on current evidence.
   - Continue, pivot, split into subtasks, research, or repair as needed.
3. **Act**
   - Implement, research, refactor, document, or spawn focused worker agents.
4. **Verify**
   - Run the smallest meaningful test/build/lint/smoke check.
   - Never claim completion without evidence.
5. **Record**
   - Update `state/autopilot.md` with what happened, what was verified, and the next likely move.
6. **Continue or Pause**
   - Continue automatically if safe and useful.
   - Pause only on a real blocker or explicit owner stop.

## Technical Boundaries

- Do not modify `source/original/`; it is the pristine upstream reference.
- Do not commit proprietary game files from `game-files/`.
- Keep each port isolated under `source/`.
- Prefer small, reviewable commits.
- Prefer reproducible tests over manual inspection.
- If a test cannot exist yet, document the temporary verification method.

## Preferred Porting Strategy

Start with understanding and preserving behavior, then modernize in layers:

1. Inventory original architecture and data formats.
2. Build parsing/loading tests around real game data where legally local.
3. Create a modern C baseline port if useful as an intermediate reference.
4. Add SDL3 platform/input/audio/video boundaries.
5. Add additional language ports once the behavior model is clear.

This strategy may change as discoveries are made. The autopilot is expected to adapt.

## TaskFlow Role

TaskFlow should store durable flow state and active child tasks. It should not require a fixed up-front task list.

The flow state should track:

- current phase
- latest verified milestone
- active hypothesis/strategy
- current blocker, if any
- next intended action
- child tasks currently running or recently completed

TaskFlow is the durable control plane; this repo is the readable project memory.
