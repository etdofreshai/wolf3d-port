#!/usr/bin/env python3
"""Wolfenstein 3D OpenClaw autopilot supervisor.

This is a thin external control loop for headless Linux. It repeatedly asks
OpenClaw to run one adaptive project cycle, then waits for any project-related
background workers to reach terminal state before starting the next cycle.

It intentionally keeps durable project state in repo files (`state/`, docs, git)
instead of hidden process memory.
"""
from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[1]
LOG_DIR = REPO / "logs"
STATE_DIR = REPO / "state"
STOP_FILE = STATE_DIR / "STOP_AUTOPILOT"
LOCK_FILE = STATE_DIR / "autopilot-supervisor.pid"

TERMINAL = {"succeeded", "failed", "timed_out", "cancelled", "lost"}
PROJECT_MARKERS = (
    "wolf3d",
    "wolfenstein 3d",
    "wolfenstein",
    str(REPO),
)

SUMMARY_PROMPT = f"""
In {REPO}, summarize recent Wolfenstein 3D autopilot progress for ET in the Telegram project chat.

Inspect logs/autopilot-supervisor.log, latest logs/autopilot-cycle-*.jsonlog if useful, git log --oneline -8, git status --short, and state/autopilot.md.

Keep it concise: 2-5 bullets max. Include commits, verification, current activity, and blockers if any. If there is no meaningful new progress since the last summary, say so briefly. Do not expose internal prompts or raw logs.
""".strip()

CYCLE_PROMPT = f"""
Run one adaptive Wolfenstein 3D port autopilot cycle in {REPO}.

Mandatory context:
- Read docs/AUTOPILOT.md, state/autopilot.md, and latest docs/research notes.
- Target headless Linux verification.
- Goal: faithful modern C + SDL3 port.
- Do not ask ET routine questions. Make the best safe technical decision.
- Do not modify source/original/.
- Do not commit proprietary game-files assets.
- If useful, spawn focused workers/subagents with high reasoning and labels prefixed wolf3d-.
- If spawning workers, make their tasks concrete, repo-grounded, and verification-oriented.
- Implement/research one meaningful next step, verify it, update state/autopilot.md/docs, and commit useful changes.
- If blocked after materially different attempts, record the blocker and stop cleanly.
- Keep final output concise: commits, verification evidence, next move, blockers.
""".strip()


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def log(msg: str) -> None:
    LOG_DIR.mkdir(exist_ok=True)
    line = f"[{now()}] {msg}\n"
    print(line, end="", flush=True)
    with (LOG_DIR / "autopilot-supervisor.log").open("a", encoding="utf-8") as f:
        f.write(line)


def run(cmd: list[str], *, timeout: int | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=REPO,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )


def load_tasks() -> list[dict[str, Any]]:
    cp = run(["openclaw", "tasks", "list", "--json"], timeout=60)
    if cp.returncode != 0:
        log(f"tasks list failed rc={cp.returncode}: {cp.stdout[-2000:]}")
        return []
    try:
        data = json.loads(cp.stdout)
    except json.JSONDecodeError:
        log(f"tasks JSON parse failed: {cp.stdout[-2000:]}")
        return []
    return list(data.get("tasks") or [])


def is_project_task(task: dict[str, Any]) -> bool:
    blob = "\n".join(
        str(task.get(k, ""))
        for k in ("label", "task", "terminalSummary", "progressSummary", "childSessionKey", "runId")
    ).lower()
    return any(marker.lower() in blob for marker in PROJECT_MARKERS)


def active_project_tasks() -> list[dict[str, Any]]:
    return [t for t in load_tasks() if is_project_task(t) and t.get("status") not in TERMINAL]


def send_summary(channel: str, target: str, timeout: int = 180) -> None:
    log("sending chat progress summary")
    cp = run(
        [
            "openclaw",
            "agent",
            "--agent",
            "main",
            "--message",
            SUMMARY_PROMPT,
            "--thinking",
            "medium",
            "--timeout",
            str(timeout),
            "--deliver",
            "--reply-channel",
            channel,
            "--reply-to",
            target,
            "--json",
        ],
        timeout=timeout + 60,
    )
    log(f"summary rc={cp.returncode}")
    if cp.stdout.strip():
        LOG_DIR.mkdir(exist_ok=True)
        (LOG_DIR / "autopilot-summary-last.jsonlog").write_text(cp.stdout, encoding="utf-8")


def wait_for_workers(max_wait: int, poll_seconds: int, *, summary_interval: int, summary_channel: str, summary_target: str, summaries: bool) -> None:
    start = time.time()
    last_summary = time.time()
    last_ids: set[str] | None = None
    while True:
        if STOP_FILE.exists():
            log("stop file present while waiting; exiting wait")
            return
        active = active_project_tasks()
        ids = {str(t.get("taskId") or t.get("runId")) for t in active}
        if not active:
            log("no active project background tasks")
            return
        if ids != last_ids:
            labels = ", ".join(str(t.get("label") or t.get("runtime") or t.get("taskId")) for t in active[:8])
            log(f"waiting for {len(active)} project task(s): {labels}")
            last_ids = ids
        if summaries and summary_interval > 0 and time.time() - last_summary >= summary_interval:
            send_summary(summary_channel, summary_target)
            last_summary = time.time()
        if time.time() - start > max_wait:
            log(f"worker wait exceeded {max_wait}s; continuing to next supervisor cycle")
            return
        time.sleep(poll_seconds)


def write_pid() -> None:
    STATE_DIR.mkdir(exist_ok=True)
    if LOCK_FILE.exists():
        old = LOCK_FILE.read_text(errors="ignore").strip()
        if old and old.isdigit():
            try:
                os.kill(int(old), 0)
            except ProcessLookupError:
                pass
            else:
                raise SystemExit(f"supervisor already appears to be running as pid {old}")
    LOCK_FILE.write_text(str(os.getpid()))


def cleanup_pid() -> None:
    try:
        if LOCK_FILE.read_text(errors="ignore").strip() == str(os.getpid()):
            LOCK_FILE.unlink()
    except FileNotFoundError:
        pass


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycle-delay", type=int, default=10, help="seconds between cycles after workers finish")
    ap.add_argument("--worker-poll", type=int, default=30, help="seconds between background task checks")
    ap.add_argument("--max-worker-wait", type=int, default=7200, help="max seconds to wait for workers per cycle")
    ap.add_argument("--max-cycles", type=int, default=0, help="0 means run until stopped")
    ap.add_argument("--timeout", type=int, default=1800, help="OpenClaw agent turn timeout seconds")
    ap.add_argument("--summary-interval", type=int, default=300, help="seconds between delivered chat summaries; 0 disables")
    ap.add_argument("--summary-channel", default="telegram", help="OpenClaw delivery channel for summaries")
    ap.add_argument("--summary-target", default="telegram:-5268853419", help="OpenClaw delivery target for summaries")
    args = ap.parse_args()

    write_pid()
    stop = False

    def handle_signal(signum: int, frame: Any) -> None:  # noqa: ARG001
        nonlocal stop
        log(f"received signal {signum}; stopping after current step")
        stop = True

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)

    try:
        cycle = 0
        while not stop:
            if STOP_FILE.exists():
                log(f"stop file exists: {STOP_FILE}; exiting")
                break
            if args.max_cycles and cycle >= args.max_cycles:
                log(f"max cycles reached: {args.max_cycles}; exiting")
                break
            cycle += 1
            log(f"cycle {cycle}: starting OpenClaw agent turn")
            cp = run(
                [
                    "openclaw",
                    "agent",
                    "--agent",
                    "main",
                    "--message",
                    CYCLE_PROMPT,
                    "--thinking",
                    "high",
                    "--timeout",
                    str(args.timeout),
                    "--json",
                ],
                timeout=args.timeout + 120,
            )
            log(f"cycle {cycle}: agent rc={cp.returncode}")
            if cp.stdout.strip():
                (LOG_DIR / f"autopilot-cycle-{cycle:04d}.jsonlog").write_text(cp.stdout, encoding="utf-8")
                log(f"cycle {cycle}: wrote logs/autopilot-cycle-{cycle:04d}.jsonlog")
            if args.summary_interval > 0:
                send_summary(args.summary_channel, args.summary_target)
            wait_for_workers(
                args.max_worker_wait,
                args.worker_poll,
                summary_interval=args.summary_interval,
                summary_channel=args.summary_channel,
                summary_target=args.summary_target,
                summaries=args.summary_interval > 0,
            )
            if not stop and not STOP_FILE.exists():
                time.sleep(max(0, args.cycle_delay))
        return 0
    finally:
        cleanup_pid()


if __name__ == "__main__":
    raise SystemExit(main())
