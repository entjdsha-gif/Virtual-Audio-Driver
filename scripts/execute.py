#!/usr/bin/env python3
"""
AO Cable V1 harness step helper.

This is a safe adaptation of the harness_framework idea.

It does not call Claude.
It does not use dangerous permission bypasses.
It does not edit source code.
It does not create branches, commits, or pushes.

It only prints the next step prompt and optionally marks a step
completed/error/blocked when the user explicitly asks it to.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone, timedelta
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
PHASES_DIR = ROOT / "phases"
KST = timezone(timedelta(hours=9))


def stamp() -> str:
    return datetime.now(KST).strftime("%Y-%m-%dT%H:%M:%S%z")


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def load_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def phase_paths(phase: str) -> tuple[Path, Path]:
    phase_dir = PHASES_DIR / phase
    index_path = phase_dir / "index.json"
    if not phase_dir.is_dir():
        raise SystemExit(f"ERROR: phase directory not found: {phase_dir}")
    if not index_path.exists():
        raise SystemExit(f"ERROR: phase index not found: {index_path}")
    return phase_dir, index_path


def find_step(index: dict[str, Any], step_num: int) -> dict[str, Any]:
    for step in index.get("steps", []):
        if step.get("step") == step_num:
            return step
    raise SystemExit(f"ERROR: step not found: {step_num}")


def next_pending_step(index: dict[str, Any]) -> dict[str, Any] | None:
    for step in index.get("steps", []):
        if step.get("status") == "pending":
            return step
    return None


def print_status(phase: str) -> None:
    _, index_path = phase_paths(phase)
    index = read_json(index_path)
    print(f"Phase: {index.get('phase', phase)}")
    print(f"Project: {index.get('project', 'AO Cable V1')}")
    print()
    for step in index.get("steps", []):
        line = f"{step['step']:>2}: {step['name']:<28} {step.get('status', 'pending')}"
        if step.get("summary"):
            line += f" - {step['summary']}"
        if step.get("blocked_reason"):
            line += f" - blocked: {step['blocked_reason']}"
        if step.get("error_message"):
            line += f" - error: {step['error_message']}"
        print(line)


def build_prompt(phase: str, step_num: int | None) -> str:
    phase_dir, index_path = phase_paths(phase)
    index = read_json(index_path)

    step = find_step(index, step_num) if step_num is not None else next_pending_step(index)
    if step is None:
        raise SystemExit("No pending steps.")

    step_file = phase_dir / f"step{step['step']}.md"
    if not step_file.exists():
        raise SystemExit(f"ERROR: step file not found: {step_file}")

    guardrail_files = [
        ROOT / "CLAUDE.md",
        ROOT / "AGENTS.md",
        ROOT / "docs" / "PRD.md",
        ROOT / "docs" / "ADR.md",
        ROOT / "docs" / "AO_CABLE_V1_ARCHITECTURE.md",
        ROOT / "docs" / "AO_CABLE_V1_DESIGN.md",
    ]

    completed = [
        f"- Step {s['step']} ({s['name']}): {s.get('summary', '')}"
        for s in index.get("steps", [])
        if s.get("status") == "completed" and s.get("summary")
    ]

    parts: list[str] = []
    parts.append("# AO Cable V2 Harness Prompt")
    parts.append("")
    parts.append("Follow CLAUDE.md first, then AGENTS.md. If anything is unknown, say it is unknown and stop rather than guessing.")
    parts.append("Do not edit files before telling the user exactly what will change.")
    parts.append("")
    parts.append("## Guardrails")
    for path in guardrail_files:
        if path.exists():
            rel = path.relative_to(ROOT)
            parts.append(f"- {rel}")
    parts.append("")

    if completed:
        parts.append("## Completed Step Summaries")
        parts.extend(completed)
        parts.append("")

    parts.append("## Current Step")
    parts.append(load_text(step_file))
    return "\n".join(parts)


def print_next(phase: str, step_num: int | None) -> None:
    print(build_prompt(phase, step_num))


def mark_step(phase: str, step_num: int, status: str, message: str) -> None:
    _, index_path = phase_paths(phase)
    index = read_json(index_path)
    step = find_step(index, step_num)

    if status not in {"pending", "completed", "error", "blocked"}:
        raise SystemExit(f"ERROR: invalid status: {status}")

    step["status"] = status
    now = stamp()

    if status == "pending":
        for key in ["completed_at", "failed_at", "blocked_at", "summary", "error_message", "blocked_reason"]:
            step.pop(key, None)
    elif status == "completed":
        step["completed_at"] = now
        step["summary"] = message or step.get("summary", "completed")
    elif status == "error":
        step["failed_at"] = now
        step["error_message"] = message or "unspecified error"
    elif status == "blocked":
        step["blocked_at"] = now
        step["blocked_reason"] = message or "unspecified blocker"

    write_json(index_path, index)
    print(f"Updated {phase} step {step_num} -> {status}")


def main() -> None:
    parser = argparse.ArgumentParser(description="AO Cable V2 harness step helper")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_status = sub.add_parser("status", help="Show phase status")
    p_status.add_argument("phase")

    p_next = sub.add_parser("next", help="Print the next pending step prompt")
    p_next.add_argument("phase")
    p_next.add_argument("--step", type=int, default=None)

    p_mark = sub.add_parser("mark", help="Mark a step status explicitly")
    p_mark.add_argument("phase")
    p_mark.add_argument("step", type=int)
    p_mark.add_argument("status", choices=["pending", "completed", "error", "blocked"])
    p_mark.add_argument("--message", default="")

    args = parser.parse_args()

    if args.cmd == "status":
        print_status(args.phase)
    elif args.cmd == "next":
        print_next(args.phase, args.step)
    elif args.cmd == "mark":
        mark_step(args.phase, args.step, args.status, args.message)


if __name__ == "__main__":
    main()
