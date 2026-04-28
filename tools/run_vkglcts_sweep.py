#!/usr/bin/env python3
"""Sprint 43 — drive `deqp-gles2` against multiple dEQP-GLES2 groups, parse
each TestResults.qpa, and emit a markdown summary table fit for inclusion
in docs/regress_report.md.

Usage:
    tools/run_vkglcts_sweep.py [--bin <path>] [--out <md>]
                               [--groups g1,g2,...] [--timeout <sec>]
                               [--include-shaders] [--include-texture]
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BIN = ROOT / "build_vkglcts" / "modules" / "gles2" / "deqp-gles2"

NAME_RE   = re.compile(r"^#beginTestCaseResult\s+(\S+)")
RESULT_RE = re.compile(r'<Result StatusCode="([^"]+)"')

# Curated default sweep — bounded test counts, broad feature coverage.
# Heavyweight groups (`shaders` ~10k cases, `texture` ~1k) are opt-in via
# CLI flags so the default run completes in a few minutes.
DEFAULT_GROUPS = [
    "dEQP-GLES2.info",
    "dEQP-GLES2.capability",
    "dEQP-GLES2.functional.prerequisite",
    "dEQP-GLES2.functional.color_clear",
    "dEQP-GLES2.functional.depth_stencil_clear",
    "dEQP-GLES2.functional.implementation_limits",
    "dEQP-GLES2.functional.buffer",
    "dEQP-GLES2.functional.fragment_ops",
    "dEQP-GLES2.functional.clip_control",
    "dEQP-GLES2.functional.light_amount",
    "dEQP-GLES2.functional.multisampled_render_to_texture",
]


def parse_qpa(path: Path) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    cur: str | None = None
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = NAME_RE.match(line)
            if m:
                cur = m.group(1)
                continue
            m = RESULT_RE.search(line)
            if m and cur is not None:
                pairs.append((cur, m.group(1)))
                cur = None
    return pairs


def run_group(bin_path: Path, group: str, timeout: int) -> dict:
    cwd = bin_path.parent
    qpa = cwd / "TestResults.qpa"
    qpa.unlink(missing_ok=True)

    cmd = [str(bin_path),
           f"--deqp-case={group}.*",
           "--deqp-log-images=disable"]
    t0 = time.time()
    try:
        rc = subprocess.run(cmd, cwd=str(cwd), capture_output=True,
                            text=True, timeout=timeout, check=False)
        timed_out = False
        ret = rc.returncode
    except subprocess.TimeoutExpired:
        timed_out = True
        ret = -1
    wall = time.time() - t0

    pairs = parse_qpa(qpa) if qpa.is_file() else []
    passed = sum(1 for _, s in pairs if s == "Pass")
    failed = sum(1 for _, s in pairs if s == "Fail")
    other  = len(pairs) - passed - failed
    return {
        "group": group,
        "total": len(pairs),
        "passed": passed,
        "failed": failed,
        "other": other,
        "wall": wall,
        "timed_out": timed_out,
        "exit": ret,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(DEFAULT_BIN))
    ap.add_argument("--out", default=str(ROOT / "out" / "vkglcts_sweep.md"))
    ap.add_argument("--groups", default="",
                    help="comma-separated list, overrides defaults")
    ap.add_argument("--timeout", type=int, default=900,
                    help="per-group timeout seconds (default 900 = 15 min)")
    ap.add_argument("--include-shaders", action="store_true",
                    help="add dEQP-GLES2.functional.shaders (~10 k cases, slow)")
    ap.add_argument("--include-texture", action="store_true",
                    help="add dEQP-GLES2.functional.texture (~1 k cases)")
    args = ap.parse_args()

    bin_path = Path(args.bin)
    if not bin_path.is_file():
        print(f"FAIL: {bin_path} not built — run tools/build_vkglcts.sh", file=sys.stderr)
        return 2

    groups = ([g.strip() for g in args.groups.split(",") if g.strip()]
              if args.groups else list(DEFAULT_GROUPS))
    if args.include_texture and "dEQP-GLES2.functional.texture" not in groups:
        groups.append("dEQP-GLES2.functional.texture")
    if args.include_shaders and "dEQP-GLES2.functional.shaders" not in groups:
        groups.append("dEQP-GLES2.functional.shaders")

    print(f"running {len(groups)} groups against {bin_path}")
    rows = []
    for g in groups:
        sys.stdout.write(f"  {g:<55s} ... ")
        sys.stdout.flush()
        r = run_group(bin_path, g, args.timeout)
        rows.append(r)
        if r["timed_out"]:
            tag = "TIMEOUT"
        elif r["total"] == 0:
            tag = "EMPTY"
        else:
            pct = 100.0 * r["passed"] / r["total"]
            tag = f"{r['passed']:>4d}/{r['total']:<4d} ({pct:5.1f}%)"
        print(f"{tag}  ({r['wall']:.1f}s)")

    # Markdown table.
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    total_pass = sum(r["passed"] for r in rows)
    total_run  = sum(r["total"]  for r in rows)
    pct_total = 100.0 * total_pass / max(1, total_run)
    with open(out_path, "w") as f:
        f.write("# VK-GL-CTS sweep — `deqp-gles2` against `gpu_glcompat`\n\n")
        f.write(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write(f"**Total: {total_pass} / {total_run} pass "
                f"({pct_total:.1f}%) across {len(rows)} groups.**\n\n")
        f.write("| group | passed | failed | other | total | wall (s) | note |\n")
        f.write("|---|---:|---:|---:|---:|---:|---|\n")
        for r in rows:
            note = "TIMEOUT" if r["timed_out"] else ("empty" if r["total"] == 0 else "")
            short = r["group"].replace("dEQP-GLES2.", "")
            f.write(f"| `{short}.*` | {r['passed']} | {r['failed']} | {r['other']} "
                    f"| {r['total']} | {r['wall']:.1f} | {note} |\n")

    print()
    print(f"report → {out_path}")
    print(f"total:  {total_pass}/{total_run} pass ({pct_total:.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
