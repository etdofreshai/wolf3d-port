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
import re
import signal
import subprocess
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[1]
LOG_DIR = REPO / "logs"
STATE_DIR = REPO / "state"
STOP_FILE = STATE_DIR / "STOP_AUTOPILOT"
LOCK_FILE = STATE_DIR / "autopilot-supervisor.pid"
MODEL_STATE_FILE = STATE_DIR / "autopilot-supervisor-model-state.json"

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

CYCLE_PROMPT_TEMPLATE = f"""
Run one adaptive Wolfenstein 3D port autopilot cycle in {REPO}.

Mandatory context:
- Read docs/AUTOPILOT.md, state/autopilot.md, and latest docs/research notes.
- Target headless Linux verification first.
- Goal: faithful modern C + SDL3 port.
- Do not ask ET routine questions. Make the best safe technical decision.
- Do not modify source/original/.
- Do not commit proprietary game-files assets.
- Do not use fast mode unless the supervisor explicitly enables it.
- Preferred model for this cycle: {{model_hint}}. If the runtime cannot switch models from this entrypoint, still use this as the review/decision-making perspective for the cycle.
- If useful, spawn focused workers/subagents with the supervisor-selected thinking level and labels prefixed wolf3d-.
- If spawning workers, make their tasks concrete, repo-grounded, and verification-oriented.
- Implement/research one meaningful next step, verify it, update state/autopilot.md/docs, and commit useful changes.
- Do not stop for ordinary blockers; pivot, research, split the task, or try materially different approaches.
- If the same issue repeats many times across materially different attempts, record the blocker and pause cleanly.
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


def parse_iso_datetime(value: str) -> datetime:
    normalized = value.strip().replace("Z", "+00:00")
    dt = datetime.fromisoformat(normalized)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc)


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


def openclaw_agent_help() -> str:
    cp = run(["openclaw", "agent", "--help"], timeout=60)
    return cp.stdout if cp.returncode == 0 else ""


def supports_agent_option(help_text: str, option: str) -> bool:
    return bool(re.search(rf"(^|\s){re.escape(option)}(\s|,|$)", help_text))


def split_models(raw: str) -> list[str]:
    return [m.strip() for m in raw.split(",") if m.strip()]


def filter_models(models: list[str], excluded: list[str]) -> list[str]:
    excluded_set = {item.lower() for item in excluded}
    return [model for model in models if model.lower() not in excluded_set and model_provider(model, "").lower() not in excluded_set]


def model_provider(model: str, fallback: str) -> str:
    if model != "runtime default" and "/" in model:
        return model.split("/", 1)[0]
    return fallback


def load_model_state() -> dict[str, Any]:
    if not MODEL_STATE_FILE.exists():
        return {"lastIndex": -1}
    try:
        data = json.loads(MODEL_STATE_FILE.read_text(encoding="utf-8"))
        if isinstance(data, dict):
            return data
    except json.JSONDecodeError:
        pass
    return {"lastIndex": -1}


def store_model_state(index: int, model: str, provider: str) -> None:
    STATE_DIR.mkdir(exist_ok=True)
    MODEL_STATE_FILE.write_text(
        json.dumps({"lastIndex": index, "model": model, "provider": provider, "updatedAt": now()}, indent=2) + "\n",
        encoding="utf-8",
    )


def model_rotation_order(models: list[str]) -> list[tuple[int, str]]:
    if not models:
        return [(0, "runtime default")]
    state = load_model_state()
    try:
        last_index = int(state.get("lastIndex", -1))
    except (ValueError, TypeError):
        last_index = -1
    return [((last_index + step) % len(models), models[(last_index + step) % len(models)]) for step in range(1, len(models) + 1)]


def select_model(models: list[str], fallback_provider: str) -> str:
    index, model = model_rotation_order(models)[0]
    store_model_state(index, model, model_provider(model, fallback_provider))
    return model


def build_agent_command(
    *,
    message: str,
    thinking: str,
    timeout: int,
    model: str,
    deliver: bool = False,
    reply_channel: str = "",
    reply_target: str = "",
    fast_mode: bool = False,
    help_text: str = "",
) -> list[str]:
    cmd = [
        "openclaw",
        "agent",
        "--agent",
        "main",
        "--message",
        message,
        "--thinking",
        thinking,
        "--timeout",
        str(timeout),
    ]
    if model != "runtime default" and supports_agent_option(help_text, "--model"):
        cmd.extend(["--model", model])
    if fast_mode:
        if supports_agent_option(help_text, "--fast"):
            cmd.append("--fast")
        elif supports_agent_option(help_text, "--fast-mode"):
            cmd.append("--fast-mode")
        else:
            log("fast mode requested but openclaw agent has no supported fast-mode flag; continuing without it")
    if deliver:
        cmd.extend(["--deliver", "--reply-channel", reply_channel, "--reply-to", reply_target])
    cmd.append("--json")
    return cmd


def parse_provider_windows(raw: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for part in raw.split(","):
        item = part.strip()
        if not item:
            continue
        if ":" not in item:
            raise ValueError(f"provider window override must be provider:window, got {item!r}")
        provider, window = item.split(":", 1)
        provider = provider.strip()
        window = window.strip()
        if not provider or not window:
            raise ValueError(f"provider window override must be provider:window, got {item!r}")
        result[provider] = window
    return result


def usage_window_for_provider(args: argparse.Namespace, provider: str) -> str:
    return args.usage_provider_windows.get(provider, args.usage_window)


def budget_start_for_window(reset_at: datetime, window_label: str) -> datetime:
    label = window_label.lower()
    if "month" in label:
        return reset_at - timedelta(days=30)
    if "week" in label:
        return reset_at - timedelta(days=7)
    match = re.search(r"(\d+)\s*h", label)
    if match:
        return reset_at - timedelta(hours=int(match.group(1)))
    return reset_at - timedelta(days=7)


def find_usage_window(status: dict[str, Any], provider: str, window_label: str) -> dict[str, Any] | None:
    usage = status.get("usage") or {}
    for item in usage.get("providers") or []:
        if str(item.get("provider")) != provider:
            continue
        for window in item.get("windows") or []:
            if str(window.get("label", "")).lower() == window_label.lower():
                return dict(window)
    return None


def load_usage_status() -> dict[str, Any] | None:
    cp = run(["openclaw", "status", "--usage", "--json"], timeout=90)
    if cp.returncode != 0:
        log(f"usage check failed rc={cp.returncode}; treating usage as unavailable: {cp.stdout[-1000:]}")
        return None
    try:
        return dict(json.loads(cp.stdout))
    except json.JSONDecodeError:
        log("usage check returned non-JSON; treating usage as unavailable")
        return None


def usage_wait_seconds_for_provider(args: argparse.Namespace, status: dict[str, Any] | None, provider: str) -> int:
    if status is None:
        return 0
    window_label = usage_window_for_provider(args, provider)
    window = find_usage_window(status, provider, window_label)
    if not window:
        log(f"usage guard: no {window_label} usage window for provider={provider}; treating provider as available")
        return 0
    used_percent = float(window.get("usedPercent") or 0)
    reset_at_ms = window.get("resetAt")
    if args.usage_budget_reset:
        reset_at = parse_iso_datetime(args.usage_budget_reset)
    elif reset_at_ms:
        reset_at = datetime.fromtimestamp(float(reset_at_ms) / 1000, tz=timezone.utc)
    else:
        log(f"usage guard: provider={provider} window={window_label} has no resetAt; treating provider as available")
        return 0
    start_at = parse_iso_datetime(args.usage_budget_start) if args.usage_budget_start else budget_start_for_window(reset_at, window_label)
    now_dt = datetime.now(timezone.utc)
    total = max((reset_at - start_at).total_seconds(), 1)
    elapsed = min(max((now_dt - start_at).total_seconds(), 0), total)
    elapsed_percent = elapsed / total * 100.0
    allowed_percent = min(100.0, max(args.usage_min_allowed_percent, elapsed_percent + args.usage_slack_percent))
    log(
        f"usage guard: provider={provider} window={window_label} "
        f"used={used_percent:.1f}% allowed={allowed_percent:.1f}% reset={reset_at.isoformat()}"
    )
    if used_percent <= allowed_percent:
        return 0
    target_elapsed_percent = max(0.0, min(100.0, used_percent - args.usage_slack_percent))
    resume_at = start_at + timedelta(seconds=total * (target_elapsed_percent / 100.0))
    wait_seconds = max(0, int((resume_at - now_dt).total_seconds()))
    log(f"usage guard: provider={provider} window={window_label} blocked until {resume_at.isoformat()} ({wait_seconds}s)")
    return wait_seconds


def select_available_model(args: argparse.Namespace, models: list[str], stop_ref: dict[str, bool]) -> str | None:
    if not args.usage_guard:
        return select_model(models, args.usage_provider)
    while not stop_ref["stop"] and not STOP_FILE.exists():
        status = load_usage_status()
        blocked: list[tuple[str, str, int]] = []
        for index, model in model_rotation_order(models):
            provider = model_provider(model, args.usage_provider)
            wait_seconds = usage_wait_seconds_for_provider(args, status, provider)
            if wait_seconds <= 0:
                store_model_state(index, model, provider)
                if blocked:
                    log(f"usage guard: skipped blocked providers before selecting model={model} provider={provider}")
                return model
            blocked.append((model, provider, wait_seconds))
        if not blocked:
            return select_model(models, args.usage_provider)
        min_wait = min(wait for _, _, wait in blocked)
        summary = ", ".join(f"{model}/{provider}:{wait}s" for model, provider, wait in blocked)
        log(f"usage guard: all configured providers blocked ({summary}); sleeping before retry")
        time.sleep(min(min_wait, args.usage_check_interval))
    return None


def send_summary(channel: str, target: str, *, thinking: str, timeout: int, help_text: str) -> None:
    log("sending chat progress summary")
    cp = run(
        build_agent_command(
            message=SUMMARY_PROMPT,
            thinking=thinking,
            timeout=timeout,
            model="runtime default",
            deliver=True,
            reply_channel=channel,
            reply_target=target,
            help_text=help_text,
        ),
        timeout=timeout + 60,
    )
    log(f"summary rc={cp.returncode}")
    if cp.stdout.strip():
        LOG_DIR.mkdir(exist_ok=True)
        (LOG_DIR / "autopilot-summary-last.jsonlog").write_text(cp.stdout, encoding="utf-8")


def wait_for_workers(
    max_wait: int,
    poll_seconds: int,
    *,
    summary_interval: int,
    summary_channel: str,
    summary_target: str,
    summary_thinking: str,
    help_text: str,
    summaries: bool,
) -> None:
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
            send_summary(summary_channel, summary_target, thinking=summary_thinking, timeout=180, help_text=help_text)
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


def next_cycle_index() -> int:
    existing = []
    for path in LOG_DIR.glob("autopilot-cycle-*.jsonlog"):
        match = re.search(r"autopilot-cycle-(\d+)\.jsonlog$", path.name)
        if match:
            existing.append(int(match.group(1)))
    return (max(existing) + 1) if existing else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycle-delay", type=int, default=10, help="seconds between cycles after workers finish")
    ap.add_argument("--worker-poll", type=int, default=30, help="seconds between background task checks")
    ap.add_argument("--max-worker-wait", type=int, default=7200, help="max seconds to wait for workers per cycle")
    ap.add_argument("--max-cycles", type=int, default=0, help="0 means run until stopped")
    ap.add_argument("--timeout", type=int, default=1800, help="OpenClaw agent turn timeout seconds")
    ap.add_argument("--thinking", default="low", help="cycle thinking level: off|minimal|low|medium|high|xhigh|adaptive|max")
    ap.add_argument("--summary-thinking", default="low", help="completion/periodic summary thinking level")
    ap.add_argument("--models", "--include-models", dest="models", default="openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1", help="comma-separated included model rotation; provider is inferred from the prefix before '/'")
    ap.add_argument("--exclude-models", default="", help="comma-separated model ids or provider prefixes to exclude from this run after includes are applied")
    ap.add_argument("--fast-mode", action=argparse.BooleanOptionalAction, default=False, help="request fast mode if the OpenClaw CLI supports it; default is off")
    ap.add_argument("--usage-guard", action=argparse.BooleanOptionalAction, default=True, help="skip over-budget model providers and pause only when every configured provider is ahead of schedule")
    ap.add_argument("--usage-provider", default="openai-codex", help="fallback provider key from openclaw status --usage for models without a provider prefix")
    ap.add_argument("--usage-window", default="Week", help="default usage window label from openclaw status --usage")
    ap.add_argument("--usage-provider-windows", default="zai:Monthly", help="comma-separated provider:window overrides, e.g. zai:Monthly,openai-codex:Week")
    ap.add_argument("--usage-budget-start", default="", help="ISO timestamp for budget start; default is inferred from the usage window label")
    ap.add_argument("--usage-budget-reset", default="", help="ISO timestamp for weekly budget reset; default uses provider resetAt")
    ap.add_argument("--usage-slack-percent", type=float, default=5.0, help="allowed percentage above the linear weekly budget curve")
    ap.add_argument("--usage-min-allowed-percent", type=float, default=3.0, help="minimum allowed percentage early in a budget window")
    ap.add_argument("--usage-check-interval", type=int, default=1800, help="seconds between usage checks while paused")
    ap.add_argument("--summary-interval", type=int, default=0, help="optional periodic summary seconds while waiting for workers; 0 disables")
    ap.add_argument("--completion-summary", action=argparse.BooleanOptionalAction, default=True, help="deliver a chat summary after each completed supervisor cycle")
    ap.add_argument("--summary-channel", default="telegram", help="OpenClaw delivery channel for summaries")
    ap.add_argument("--summary-target", default="telegram:-5268853419", help="OpenClaw delivery target for summaries")
    args = ap.parse_args()
    args.usage_provider_windows = parse_provider_windows(args.usage_provider_windows)

    write_pid()
    stop_ref = {"stop": False}
    help_text = openclaw_agent_help()
    excluded_models = split_models(args.exclude_models)
    models = filter_models(split_models(args.models), excluded_models)
    if not models:
        raise SystemExit("all configured models were excluded; adjust --models or --exclude-models")

    def handle_signal(signum: int, frame: Any) -> None:  # noqa: ARG001
        log(f"received signal {signum}; stopping after current step")
        stop_ref["stop"] = True

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)

    try:
        cycle_count = 0
        cycle_log_index = next_cycle_index()
        while not stop_ref["stop"]:
            if STOP_FILE.exists():
                log(f"stop file exists: {STOP_FILE}; exiting")
                break
            if args.max_cycles and cycle_count >= args.max_cycles:
                log(f"max cycles reached: {args.max_cycles}; exiting")
                break
            selected_model = select_available_model(args, models, stop_ref)
            if selected_model is None or stop_ref["stop"] or STOP_FILE.exists():
                break
            cycle_count += 1
            cycle_prompt = CYCLE_PROMPT_TEMPLATE.format(model_hint=selected_model)
            log(f"cycle {cycle_count}: starting OpenClaw agent turn thinking={args.thinking} model_hint={selected_model} fast_mode={args.fast_mode}")
            cp = run(
                build_agent_command(
                    message=cycle_prompt,
                    thinking=args.thinking,
                    timeout=args.timeout,
                    model=selected_model,
                    fast_mode=args.fast_mode,
                    help_text=help_text,
                ),
                timeout=args.timeout + 120,
            )
            log(f"cycle {cycle_count}: agent rc={cp.returncode}")
            if cp.stdout.strip():
                log_path = LOG_DIR / f"autopilot-cycle-{cycle_log_index:04d}.jsonlog"
                log_path.write_text(cp.stdout, encoding="utf-8")
                log(f"cycle {cycle_count}: wrote {log_path.relative_to(REPO)}")
                cycle_log_index += 1
            wait_for_workers(
                args.max_worker_wait,
                args.worker_poll,
                summary_interval=args.summary_interval,
                summary_channel=args.summary_channel,
                summary_target=args.summary_target,
                summary_thinking=args.summary_thinking,
                help_text=help_text,
                summaries=args.summary_interval > 0,
            )
            if args.completion_summary:
                send_summary(args.summary_channel, args.summary_target, thinking=args.summary_thinking, timeout=180, help_text=help_text)
            if not stop_ref["stop"] and not STOP_FILE.exists():
                time.sleep(max(0, args.cycle_delay))
        return 0
    finally:
        cleanup_pid()


if __name__ == "__main__":
    raise SystemExit(main())
