#!/usr/bin/env python3
"""Run one or more parallel Wolf3D autopilot waves using git worktrees."""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[1]
SUPERVISOR = REPO / "scripts" / "wolf3d_autopilot_supervisor.py"
WORKTREES_DIR = REPO / ".worktrees"
LOG_DIR = REPO / "logs"
WAVE_STATE_FILE = REPO / "state" / "autopilot-parallel-wave-state.json"


def load_supervisor() -> Any:
    spec = importlib.util.spec_from_file_location("wolf3d_supervisor", SUPERVISOR)
    if not spec or not spec.loader:
        raise RuntimeError(f"could not load supervisor: {SUPERVISOR}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(cmd: list[str], *, cwd: Path = REPO, timeout: int | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=timeout)


def parse_retry_delays(value: str) -> list[int]:
    delays: list[int] = []
    for item in (value or "").split(","):
        item = item.strip()
        if not item:
            continue
        try:
            delay = int(item)
        except ValueError:
            raise SystemExit(f"invalid retry delay: {item!r}")
        if delay < 0:
            raise SystemExit(f"retry delay must be non-negative: {item!r}")
        delays.append(delay)
    return delays


def is_retryable_agent_start_failure(returncode: int, output: str) -> bool:
    if returncode == 0:
        return False
    retryable_needles = (
        "Failed to start CLI",
        "PluginLoadFailureError",
        "failed to install bundled runtime deps",
        "ENOTEMPTY",
        "EEXIST",
        "EBUSY",
        "plugin load failed",
    )
    return any(needle in (output or "") for needle in retryable_needles)


def run_agent_command_with_retries(cmd: list[str], *, cwd: Path, timeout: int, retry_delays: list[int], log_prefix: Path | None = None) -> tuple[int, str, int, float]:
    outputs: list[str] = []
    started = time.time()
    attempts = max(1, len(retry_delays) + 1)
    for attempt in range(1, attempts + 1):
        proc = subprocess.Popen(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out, _ = proc.communicate(timeout=timeout)
        out = out or ""
        if log_prefix is not None:
            log_prefix.parent.mkdir(parents=True, exist_ok=True)
            log_prefix.with_name(f"{log_prefix.name}-attempt-{attempt}.jsonlog").write_text(out, encoding="utf-8")
        outputs.append(f"--- attempt {attempt}/{attempts} rc={proc.returncode} ---\n{out}")
        if proc.returncode == 0:
            return proc.returncode, "\n".join(outputs), attempt, time.time() - started
        if attempt >= attempts or not is_retryable_agent_start_failure(proc.returncode, out):
            return proc.returncode, "\n".join(outputs), attempt, time.time() - started
        delay = retry_delays[attempt - 1]
        outputs.append(f"Retrying retryable OpenClaw agent startup failure after {delay}s.")
        time.sleep(delay)
    return 1, "\n".join(outputs), attempts, time.time() - started


def slug_model(model: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", model).strip("-").lower()




def load_wave_state() -> dict[str, Any]:
    if not WAVE_STATE_FILE.exists():
        return {"lastWaveNumber": 0}
    try:
        data = json.loads(WAVE_STATE_FILE.read_text(encoding="utf-8"))
        if isinstance(data, dict):
            data.setdefault("lastWaveNumber", 0)
            return data
    except json.JSONDecodeError:
        pass
    return {"lastWaveNumber": 0}


def next_wave_number() -> int:
    state = load_wave_state()
    try:
        number = int(state.get("lastWaveNumber", 0)) + 1
    except (TypeError, ValueError):
        number = 1
    WAVE_STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    WAVE_STATE_FILE.write_text(json.dumps({"lastWaveNumber": number, "updatedAt": time.time()}, indent=2) + "\n", encoding="utf-8")
    return number


def extract_worker_text(json_text: str) -> str:
    try:
        data = json.loads(json_text)
        payloads = (((data.get("result") or {}).get("payloads")) or [])
        texts = [p.get("text", "") for p in payloads if isinstance(p, dict) and p.get("text")]
        if texts:
            return "\n".join(texts)
    except Exception:
        pass
    return json_text or ""


def summarize_worker_output(json_text: str) -> str:
    text = extract_worker_text(json_text)
    lines = [line.strip() for line in text.splitlines()]
    for heading in ("Work done:", "Accomplished:", "Review findings:"):
        for i, line in enumerate(lines):
            if line.lower() == heading.lower():
                bullets = []
                for nxt in lines[i + 1:i + 5]:
                    if not nxt:
                        continue
                    if nxt.endswith(":") and not nxt.startswith("-"):
                        break
                    bullets.append(nxt.lstrip("- "))
                    if len(bullets) >= 2:
                        break
                if bullets:
                    return "; ".join(bullets)
    for line in lines:
        if line.startswith("-"):
            return line.lstrip("- ")
    return "completed" if text else "no worker summary"

def git_head() -> str:
    cp = run(["git", "rev-parse", "--short", "HEAD"])
    return cp.stdout.strip() if cp.returncode == 0 else "unknown"


def git_full_head() -> str:
    cp = run(["git", "rev-parse", "HEAD"])
    return cp.stdout.strip() if cp.returncode == 0 else ""


def build_prompt(sup: Any, args: argparse.Namespace, model: str, wave_id: str, worktree: Path) -> str:
    models = args.models_list
    review = sup.build_review_instructions(args, models, model)
    return f"""
Run one parallel-wave Wolfenstein 3D port autopilot task in this isolated git worktree: {worktree}

Wave: {wave_id}
Preferred model for this worker: {model}. If runtime model switching is unavailable, use this as your review/decision-making perspective.

Rules:
- Work only inside this worktree: {worktree}
- Do not edit the main worktree at {REPO}.
- Read docs/AUTOPILOT.md, state/autopilot.md, state/parallel-wave-notes/ if present, and relevant docs/research notes.
- Target headless Linux verification first.
- Do not modify source/original/.
- Do not commit proprietary game-files assets.
- Do not push.
- Keep the task small and test-backed.
{review}
- Choose one useful next porting step that minimizes likely merge conflicts with other parallel workers.
- Run the best relevant verification, usually `cd source/modern-c-sdl3 && make test` and SDL3 targets when relevant.
- Do not edit state/autopilot.md directly in parallel wave mode; it is merge-hot. Record review findings and progress in state/parallel-wave-notes/{wave_id}-{slug_model(model)}.md instead, and update relevant docs/code/tests as needed.
- Commit useful changes locally on this worktree branch.
- Final output: commit hash, files changed, verification, blockers, and any review findings.
""".strip()


def eligible_models(sup: Any, args: argparse.Namespace, stop_ref: dict[str, bool]) -> list[str]:
    if not args.usage_guard:
        return args.models_list[: args.parallel_max_models]
    status = sup.load_usage_status()
    selected: list[str] = []
    blocked: list[dict[str, Any]] = []
    for _, model in sup.model_rotation_order(args.models_list):
        provider = sup.model_provider(model, args.usage_provider)
        checks = sup.usage_checks_for_provider(args, status, provider)
        wait_seconds = max((int(check.get("waitSeconds") or 0) for check in checks), default=0)
        if wait_seconds <= 0:
            selected.append(model)
            if len(selected) >= args.parallel_max_models:
                break
        else:
            blocked.append({"model": model, "provider": provider, "waitSeconds": wait_seconds, "checks": checks})
    if blocked:
        sup.send_usage_skip_update(args, blocked, selected[0] if selected else None, args.help_text)
    return selected


def create_worktree(branch: str, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        run(["git", "worktree", "remove", "--force", str(path)], timeout=120)
    cp = run(["git", "worktree", "add", "-B", branch, str(path), "HEAD"], timeout=180)
    if cp.returncode != 0:
        raise RuntimeError(cp.stdout)


def branch_has_commits(branch: str, base: str) -> bool:
    cp = run(["git", "rev-list", "--count", f"{base}..{branch}"])
    return cp.returncode == 0 and cp.stdout.strip() not in ("", "0")


def merge_branch(branch: str) -> tuple[bool, str]:
    cp = run(["git", "merge", "--no-ff", branch, "-m", f"Merge {branch}"], timeout=240)
    if cp.returncode == 0:
        return True, cp.stdout
    return False, cp.stdout


def merge_in_progress() -> bool:
    cp = run(["git", "rev-parse", "-q", "--verify", "MERGE_HEAD"], timeout=30)
    return cp.returncode == 0


def resolve_merge_conflict(args: argparse.Namespace, branch: str, model: str, merge_output: str, wave_id: str) -> tuple[bool, str]:
    if not args.resolve_merge_conflicts:
        run(["git", "merge", "--abort"], timeout=120)
        return False, merge_output
    combined_output = []
    resolver_models = [m for m in args.models_list if m != model] + [model]
    resolver_models = resolver_models[: max(1, args.merge_resolver_attempts)]
    for attempt, resolver_model in enumerate(resolver_models, start=1):
        prompt = f"""
Aggressively resolve the current git merge conflict in {REPO}.

Context:
- Parallel wave: {wave_id}
- Branch being merged: {branch}
- Model branch owner: {model}
- Resolver attempt: {attempt}/{len(resolver_models)}
- Preferred resolver model perspective: {resolver_model}
- Merge output:
{merge_output}

Instructions:
- Goal: avoid human intervention if at all safely possible.
- Inspect `git status`, conflicted files, and both sides of the conflict.
- Preserve useful changes from both main and {branch}; when files are additive docs/state/test notes, combine them.
- Prefer resolving merge-hot generated/progress notes by keeping both non-duplicative entries.
- Do not modify source/original/.
- Do not add proprietary game data.
- Run verification, at minimum `cd source/modern-c-sdl3 && make test`; include `make test-sdl3` when SDL3/presentation files are touched and available.
- Complete the merge commit if resolution succeeds.
- Only abort/report blocker if the conflict is genuinely unsafe or verification cannot be fixed.

Final output: files resolved, verification, final commit/head, or blocker.
""".strip()
        cmd = [
            "openclaw", "agent", "--agent", "main",
            "--session-id", f"wolf3d-wave-{wave_id}-merge-resolver-{attempt}",
            "--message", prompt, "--thinking", args.merge_resolver_thinking,
            "--timeout", str(args.timeout), "--json",
        ]
        log_path = LOG_DIR / f"parallel-wave-{wave_id}-{slug_model(model)}-resolver-{attempt}.jsonlog"
        returncode, output, cli_attempts, _duration = run_agent_command_with_retries(
            cmd,
            cwd=REPO,
            timeout=args.timeout + 180,
            retry_delays=args.worker_retry_delays_list,
            log_prefix=LOG_DIR / f"parallel-wave-{wave_id}-{slug_model(model)}-resolver-{attempt}",
        )
        combined_output.append(output)
        log_path.write_text(output, encoding="utf-8")
        if returncode != 0:
            continue
        if merge_in_progress():
            combined_output.append("Resolver exited but MERGE_HEAD still exists; trying another resolver if available.")
            continue
        verify = run(["make", "test"], cwd=REPO / "source" / "modern-c-sdl3", timeout=300)
        (LOG_DIR / f"parallel-wave-{wave_id}-{slug_model(model)}-resolver-{attempt}-verify.log").write_text(verify.stdout, encoding="utf-8")
        if verify.returncode == 0:
            return True, "\n".join(combined_output)
        combined_output.append(verify.stdout)
    if merge_in_progress():
        run(["git", "merge", "--abort"], timeout=120)
    return False, "\n".join(combined_output)


def main() -> int:
    sup = load_supervisor()
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-waves", type=int, default=1, help="number of parallel waves to run; 0 means until stopped")
    ap.add_argument("--parallel-max-models", type=int, default=3)
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--thinking", default="low")
    ap.add_argument("--models", "--include-models", dest="models", default="openai-codex/gpt-5.5,anthropic/claude-opus-4.7,zai/glm-5.1")
    ap.add_argument("--exclude-models", default="")
    ap.add_argument("--review-previous-steps", action=argparse.BooleanOptionalAction, default=True)
    ap.add_argument("--review-fallback-commit-count", type=int, default=3)
    ap.add_argument("--usage-guard", action=argparse.BooleanOptionalAction, default=True)
    ap.add_argument("--usage-provider", default="openai-codex")
    ap.add_argument("--usage-window", default="Week")
    ap.add_argument("--usage-provider-windows", default="zai:Monthly")
    ap.add_argument("--usage-extra-windows", default="5h")
    ap.add_argument("--usage-skip-updates", action=argparse.BooleanOptionalAction, default=True)
    ap.add_argument("--usage-budget-start", default="")
    ap.add_argument("--usage-budget-reset", default="")
    ap.add_argument("--usage-slack-percent", type=float, default=5.0)
    ap.add_argument("--usage-min-allowed-percent", type=float, default=3.0)
    ap.add_argument("--summary-thinking", default="low")
    ap.add_argument("--summary-channel", default="telegram")
    ap.add_argument("--summary-target", default="telegram:-5268853419")
    ap.add_argument("--resolve-merge-conflicts", action=argparse.BooleanOptionalAction, default=True, help="ask agents to resolve merge conflicts before failing a wave")
    ap.add_argument("--merge-resolver-attempts", type=int, default=3, help="maximum resolver model attempts before giving up")
    ap.add_argument("--merge-resolver-thinking", default="medium", help="thinking level for merge resolver agents")
    ap.add_argument("--push-after-wave", action=argparse.BooleanOptionalAction, default=True)
    ap.add_argument("--cleanup-worktrees", action=argparse.BooleanOptionalAction, default=True)
    ap.add_argument("--worker-retry-delays", default="5,10,15,30", help="comma-separated seconds to wait before retrying retryable OpenClaw agent startup failures")
    args = ap.parse_args()

    args.usage_provider_windows = sup.parse_provider_windows(args.usage_provider_windows)
    args.usage_extra_windows = sup.parse_csv(args.usage_extra_windows)
    args.help_text = sup.openclaw_agent_help()
    args.worker_retry_delays_list = parse_retry_delays(args.worker_retry_delays)
    excluded = sup.split_models(args.exclude_models)
    args.models_list = sup.filter_models(sup.split_models(args.models), excluded)
    if not args.models_list:
        raise SystemExit("all configured models were excluded")

    waves_run = 0
    while args.max_waves == 0 or waves_run < args.max_waves:
        if sup.STOP_FILE.exists() or sup.STOP_AFTER_CYCLE_FILE.exists():
            print("stop file present; exiting parallel wave runner")
            break
        waves_run += 1
        wave_number = next_wave_number()
        wave_id = time.strftime("%Y%m%d-%H%M%S")
        wave_label = f"wave {wave_number}"
        base = git_full_head()
        models = eligible_models(sup, args, {"stop": False})
        if not models:
            print("no eligible models for this wave")
            break
        sup.send_plain_update(args, f"Starting Wolf3D parallel wave {wave_number}.\nModels: {', '.join(models)}.", args.help_text, "parallel wave start")
        workers = []
        for model in models:
            slug = slug_model(model)
            branch = f"autopilot/wave-{wave_id}/{slug}"
            wt = WORKTREES_DIR / f"wave-{wave_id}" / slug
            create_worktree(branch, wt)
            prompt = build_prompt(sup, args, model, wave_id, wt)
            log_path = LOG_DIR / f"parallel-wave-{wave_id}-{slug}.jsonlog"
            cmd = ["openclaw", "agent", "--agent", "main", "--session-id", f"wolf3d-wave-{wave_id}-{slug}",
                   "--message", prompt, "--thinking", args.thinking, "--timeout", str(args.timeout), "--json"]
            proc = subprocess.Popen(cmd, cwd=wt, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            workers.append({"model": model, "slug": slug, "branch": branch, "worktree": wt, "cmd": cmd, "proc": proc, "log": log_path, "startedAt": time.time()})
        results = []
        for worker in workers:
            out, _ = worker["proc"].communicate(timeout=args.timeout + 180)
            out = out or ""
            returncode = int(worker["proc"].returncode or 0)
            attempts = 1
            duration = time.time() - float(worker.get("startedAt", time.time()))
            combined_output = f"--- attempt 1/{len(args.worker_retry_delays_list) + 1} rc={returncode} ---\n{out}"
            if is_retryable_agent_start_failure(returncode, out):
                retry_returncode, retry_output, retry_attempts, retry_duration = run_agent_command_with_retries(
                    worker["cmd"],
                    cwd=worker["worktree"],
                    timeout=args.timeout + 180,
                    retry_delays=args.worker_retry_delays_list,
                    log_prefix=worker["log"].with_suffix(""),
                )
                combined_output = combined_output + "\n" + retry_output
                returncode = retry_returncode
                attempts += retry_attempts
                duration += retry_duration
            worker["log"].write_text(combined_output, encoding="utf-8")
            summary = summarize_worker_output(combined_output)
            if attempts > 1:
                summary = f"attempts={attempts}; {summary}"
            results.append({**worker, "returncode": returncode, "output": combined_output, "duration": duration, "attempts": attempts, "summary": summary})
        merged = []
        conflicts = []
        model_lines = []
        for result in results:
            branch = result["branch"]
            model = result["model"]
            duration_text = sup.format_duration(float(result.get("duration", 0)))
            model_lines.append(f"{model}: {duration_text}; {result.get('summary', 'completed')}")
            if result["returncode"] != 0:
                conflicts.append(f"{model}: worker rc={result['returncode']}")
                continue
            if not branch_has_commits(branch, base):
                conflicts.append(f"{model}: no commit produced")
                continue
            ok, output = merge_branch(branch)
            if ok:
                merged.append(f"{model}@{git_head()}")
                sup.record_model_commit(model, git_full_head())
            else:
                (LOG_DIR / f"parallel-wave-{wave_id}-{result['slug']}-merge-failure.log").write_text(output, encoding="utf-8")
                resolved, resolver_output = resolve_merge_conflict(args, branch, model, output, wave_id)
                if resolved:
                    merged.append(f"{model}@{git_head()} (resolved)")
                    sup.record_model_commit(model, git_full_head())
                else:
                    conflicts.append(f"{model}: merge conflict/failure")
                    (LOG_DIR / f"parallel-wave-{wave_id}-{result['slug']}-resolver-failure.log").write_text(resolver_output, encoding="utf-8")
        verify = run(["make", "test"], cwd=REPO / "source" / "modern-c-sdl3", timeout=300)
        (LOG_DIR / f"parallel-wave-{wave_id}-verify.log").write_text(verify.stdout, encoding="utf-8")
        pushed = False
        if verify.returncode == 0 and args.push_after_wave:
            pushed = sup.git_push_current_branch()
        if args.cleanup_worktrees:
            for result in results:
                run(["git", "worktree", "remove", "--force", str(result["worktree"])], timeout=120)
        text = (f"Wolf3D parallel wave {wave_number} complete.\n"
                f"Models: {' | '.join(model_lines) or 'none'}.\n"
                f"Merged: {', '.join(merged) or 'none'}.\n"
                f"Issues: {', '.join(conflicts) or 'none'}.\n"
                f"Verification: {'passed' if verify.returncode == 0 else 'failed'}. Push: {'yes' if pushed else 'no'}. Head: {git_head()}.")
        sup.send_plain_update(args, text, args.help_text, "parallel wave complete")
        if verify.returncode != 0 or conflicts:
            break
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
