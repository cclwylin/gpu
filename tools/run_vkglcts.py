#!/usr/bin/env python3
"""Sprint 42 — wrapper that runs deqp-gles2 against a case filter, parses
TestResults.qpa, and exits non-zero if the pass count drops below the
expected baseline. Used by the `vkglcts.gles2.color_clear` ctest entry.

Usage:
    tools/run_vkglcts.py <case_filter> --min-passes N [--total N]
                         [--bin <path-to-deqp-gles2>]

Today's color_clear baseline (sw_ref): 8/19 passes. Failures cluster on
scissor + colorMask tests — gaps tracked in PROGRESS.md follow-ups #14
(glColorMask) and the scissored-clear note in glcompat_render.cpp.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BIN = ROOT / "build_vkglcts" / "modules" / "gles2" / "deqp-gles2"

# qpa lines we care about: alternating between "#beginTestCaseResult NAME"
# (text format) and "<Result StatusCode=..." (xml inside <TestCaseResult>).
RESULT_RE = re.compile(r'<Result StatusCode="([^"]+)"')
NAME_RE   = re.compile(r"^#beginTestCaseResult\s+(\S+)")


def parse_qpa(path: Path) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    cur_name: str | None = None
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = NAME_RE.match(line)
            if m:
                cur_name = m.group(1)
                continue
            m = RESULT_RE.search(line)
            if m and cur_name is not None:
                pairs.append((cur_name, m.group(1)))
                cur_name = None
    return pairs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("case_filter", help="dEQP case filter, e.g. dEQP-GLES2.functional.color_clear.*")
    ap.add_argument("--min-passes", type=int, required=True,
                    help="minimum passing test count (regression bar)")
    ap.add_argument("--total", type=int, default=0,
                    help="expected total test count (warns on drift)")
    ap.add_argument("--bin", default=str(DEFAULT_BIN), help="path to deqp-gles2")
    ap.add_argument("--qpa", default="TestResults.qpa", help="qpa log path")
    ap.add_argument("--no-images", action="store_true", default=True,
                    help="pass --deqp-log-images=disable (default true)")
    args = ap.parse_args()

    bin_path = Path(args.bin)
    if not bin_path.is_file():
        print(f"FAIL: deqp-gles2 not built — run tools/build_vkglcts.sh first", file=sys.stderr)
        return 2

    # deqp-gles2 always writes the qpa relative to its CWD; run from the
    # build dir so the log lands somewhere predictable.
    cwd = bin_path.parent
    qpa_path = cwd / args.qpa
    qpa_path.unlink(missing_ok=True)

    cmd = [str(bin_path), f"--deqp-case={args.case_filter}"]
    if args.no_images:
        cmd.append("--deqp-log-images=disable")

    rc = subprocess.run(cmd, cwd=str(cwd), check=False)
    # deqp-gles2 exits non-zero when ANY test fails — that's expected for our
    # subset (today: 11/19 fail in color_clear). Only treat it as fatal when
    # we also failed to produce a parseable QPA log.
    if not qpa_path.is_file():
        print(f"FAIL: no qpa log at {qpa_path} (deqp-gles2 exit={rc.returncode})", file=sys.stderr)
        return 1
    if rc.returncode not in (0, 1):
        print(f"FAIL: deqp-gles2 exited unexpectedly with {rc.returncode}", file=sys.stderr)
        return 1

    pairs = parse_qpa(qpa_path)
    passed = sum(1 for _, s in pairs if s == "Pass")
    failed = sum(1 for _, s in pairs if s == "Fail")
    other  = len(pairs) - passed - failed

    print(f"[vkglcts] filter={args.case_filter}")
    print(f"[vkglcts] passed={passed}  failed={failed}  other={other}  total={len(pairs)}")
    if args.total and len(pairs) != args.total:
        print(f"[vkglcts] WARN: total {len(pairs)} != expected {args.total}", file=sys.stderr)
    if passed < args.min_passes:
        print(f"FAIL: passed {passed} < min-passes {args.min_passes}", file=sys.stderr)
        return 1
    print(f"OK: passed {passed} >= min-passes {args.min_passes}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
