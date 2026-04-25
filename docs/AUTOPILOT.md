# Wolfenstein 3D Port Autopilot

This project is intended to run as an adaptive long-running OpenClaw/TaskFlow-driven effort.

## Owner Intent

Port Wolfenstein 3D to modern C using SDL3, starting from the original id Software source and legally supplied local game data.

The owner wants to specify intent with minimal prompting. The autopilot should make reasonable technical decisions independently and keep moving toward the goal.

## Prime Directive

Make the best next move toward a working, verified modern C + SDL3 port. Do not wait for perfect instructions. Prefer useful progress over asking for clarification. Run continuously until the owner explicitly stops the autopilot, a safety/legal/destructive/public-action decision requires owner approval, or the same issue has repeated many times across materially different attempts.

## Ask/Escalate Only When Necessary

Do **not** ask the owner for routine decisions. Take the best guess and record the rationale.

Escalate only for:

- Legal/licensing uncertainty beyond normal local-use assumptions
- Destructive operations or irreversible data loss
- External/public publishing, messaging, or uploads
- A true product-direction fork where both options are expensive and mutually exclusive
- Missing credentials/assets that cannot be inferred or worked around
- Repeated verification failure after many materially different attempts, usually in the tens for the same issue rather than a handful of failures

Everything else should be handled autonomously.

## Runtime Environment

The autopilot runs on a headless Linux machine. All verification must target headless Linux first.

Implications:

- Prefer CLI tests, unit tests, parsers, build checks, and deterministic file/output validation.
- Do not require an interactive display for normal verification.
- SDL3 work should support headless-friendly smoke tests where possible, using dummy/offscreen drivers or non-rendering seams before graphical inspection.
- Visual tests are desired, but they should usually be screenshot/offscreen artifacts that can be inspected or compared from headless Linux.
- Deterministic CLI tests come first; SDL3 presentation and screenshot tests should be added once the underlying behavior has useful characterization tests.
- Long-running cycles should continue autonomously overnight and avoid pausing for routine clarification.

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
   - Pause only for owner stop, safety/legal/destructive/public-action approval needs, budget guardrails, or repeated same-issue failure after many materially different attempts. Otherwise choose the next best step and continue.

## Technical Boundaries

- Do not modify `source/original/`; it is the pristine upstream reference.
- Do not commit proprietary game files from `game-files/`.
- Keep each port isolated under `source/`.
- Prefer small, reviewable commits.
- Prefer reproducible tests over manual inspection.
- If a test cannot exist yet, document the temporary verification method.

## Preferred Porting Strategy

Primary target: modern C using SDL3. Do not pursue C#, Rust, or other language ports unless the owner changes direction.

Hard constraint: stay as close to the original source behavior and structure as practical. Prefer faithful behavior over idiomatic rewrites.

Use tests as the bridge between original and modern code:

1. Inventory original architecture and data formats.
2. Identify units that can be characterized with tests: data loading, map parsing, fixed-point/math helpers, state transitions, rendering preparation, input translation, audio/music behavior.
3. Where practical, build tests that can validate both the original-derived behavior and the modern implementation.
4. Keep `source/original/` untouched. Add any harnesses, extracted fixtures, or adapters outside the original submodule.
5. Build the modern C + SDL3 implementation under `source/modern-c-sdl3/`.
6. Add SDL3 platform/input/audio/video boundaries after core behavior is characterized, with headless/offscreen screenshot tests when visual evidence becomes useful.

This strategy may change as discoveries are made. The autopilot is expected to adapt, but the target remains modern C + SDL3 unless changed by the owner.

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
