#!/usr/bin/env python3
"""Regress every tests/examples/*.c through sw_ref + SC chain.

Generates a markdown report at out/regress_report.md and prints a
summary table to stdout. Each example is treated as 4 independent
checks:
  * builds       (glex_<name> compiles)
  * runs         (glex_<name> exits 0, writes PPM)
  * sc-runs      (sc_pattern_runner exits 0, writes PPM)
  * matches      (RMSE swref↔sc < threshold; default 1.0)

Usage:
    python3 tools/regress_examples.py [--fast] [--rmse-max FLOAT]

--fast     skip the SC pass (sw_ref only — much quicker)
--rmse-max threshold for "matches" column (default 1.0)
"""
from __future__ import annotations

import argparse
import math
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EX_DIR = ROOT / "tests" / "examples"
OUT_DIR = ROOT / "out"
BUILD_DIR = ROOT / "build"
SYSTEMC_HOME = Path.home() / ".local" / "systemc-2.3.4-gcc"

KV_RE = re.compile(r"^([A-Z_]+)=(.+)$")
HELPERS = {"logo", "mjkimage", "trackball"}


def parse_kv(s: str) -> dict[str, str]:
    return {m.group(1): m.group(2)
            for m in (KV_RE.match(ln.strip()) for ln in s.splitlines()) if m}


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    nl, parts, cur = 0, [], bytearray()
    while len(parts) < 3 and nl < len(data):
        b = data[nl]; nl += 1
        if b in (0x0A, 0x20):
            if cur:
                parts.append(bytes(cur).decode()); cur = bytearray()
        else:
            cur.append(b)
    if not parts or parts[0] != "P6":
        raise ValueError(f"{path}: not a P6 PPM")
    W, H = int(parts[1]), int(parts[2])
    head_end = data.index(b"255\n") + 4
    return W, H, data[head_end:]


def painted(buf: bytes) -> int:
    return sum(1 for i in range(0, len(buf) - 2, 3)
               if buf[i] | buf[i + 1] | buf[i + 2])


def rmse(a: bytes, b: bytes) -> float:
    if len(a) != len(b):
        return float("inf")
    s = 0.0
    for ca, cb in zip(a, b):
        d = ca - cb
        s += d * d
    return math.sqrt(s / len(a))


def run_one(name: str, env: dict, do_sc: bool) -> dict:
    """Run one example end-to-end. Returns a row dict."""
    sw_ppm    = OUT_DIR / f"glex_{name}.ppm"
    scene_out = OUT_DIR / f"glex_{name}.scene"
    sc_ppm    = OUT_DIR / f"glex_{name}.sc.ppm"

    row = {
        "name": name,
        "builds": False,
        "runs": False,
        "sc_runs": False,
        "sw_paint": 0,
        "sc_paint": 0,
        "tris": 0,
        "rmse": None,
        "cycles": 0,
        "wall_ms": 0,
        "note": "",
    }

    # 1. cmake build.
    bin_path = BUILD_DIR / "glcompat" / f"glex_{name}"
    rc = subprocess.run(
        ["cmake", "--build", str(BUILD_DIR),
         "--target", f"glex_{name}", "-j", "4"],
        capture_output=True, text=True, check=False)
    if rc.returncode != 0 or not bin_path.is_file():
        row["note"] = "build fail"
        return row
    row["builds"] = True

    # 2. Run glcompat binary (sw_ref render + scene capture).
    sw_ppm.unlink(missing_ok=True)
    scene_out.unlink(missing_ok=True)
    env_g = dict(env)
    env_g["GLCOMPAT_OUT"]   = str(sw_ppm)
    env_g["GLCOMPAT_SCENE"] = str(scene_out)
    t0 = time.time()
    rc = subprocess.run([str(bin_path)], capture_output=True, text=True,
                        env=env_g, check=False, timeout=30)
    if rc.returncode != 0 or not sw_ppm.is_file():
        row["note"] = "run fail"
        return row
    row["runs"] = True
    try:
        W, H, sw_buf = read_ppm(sw_ppm)
        row["sw_paint"] = painted(sw_buf)
    except Exception:
        row["note"] = "ppm parse"; return row

    if not do_sc:
        return row

    # 3. SC pattern runner.
    sc_bin = BUILD_DIR / "tests" / "conformance" / "sc_pattern_runner"
    if not sc_bin.is_file() or not scene_out.is_file():
        row["note"] = "no scene"; return row
    rc = subprocess.run([str(sc_bin), str(scene_out), str(sc_ppm)],
                        capture_output=True, text=True,
                        env=env_g, check=False, timeout=60)
    row["wall_ms"] = int((time.time() - t0) * 1000)
    if rc.returncode != 0 or not sc_ppm.is_file():
        row["note"] = f"sc fail (exit={rc.returncode})"
        return row
    row["sc_runs"] = True
    kv = parse_kv(rc.stdout)
    row["cycles"] = int(kv.get("CYCLES", "0"))
    row["sc_paint"] = int(kv.get("PAINTED", "0"))
    row["tris"] = int(kv.get("TRIANGLES", "0"))
    try:
        sw_W, sw_H, sw_buf2 = read_ppm(sw_ppm)
        sc_W, sc_H, sc_buf = read_ppm(sc_ppm)
        if (sw_W, sw_H) == (sc_W, sc_H):
            row["rmse"] = rmse(sw_buf2, sc_buf)
    except Exception:
        pass
    return row


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--fast", action="store_true",
                    help="skip the SC chain pass (sw_ref only)")
    ap.add_argument("--rmse-max", type=float, default=1.0,
                    help="RMSE threshold for the 'match' column (default 1.0)")
    ap.add_argument("--out", default=str(OUT_DIR / "regress_report.md"))
    args = ap.parse_args()

    OUT_DIR.mkdir(exist_ok=True)
    examples = sorted(p.stem for p in EX_DIR.glob("*.c") if p.stem not in HELPERS)
    print(f"running {len(examples)} examples (do_sc={not args.fast}) ...")

    env = dict(os.environ)
    env["DYLD_LIBRARY_PATH"] = (
        f"{SYSTEMC_HOME}/lib:" + env.get("DYLD_LIBRARY_PATH", ""))

    rows = []
    for i, name in enumerate(examples):
        print(f"  [{i+1:>2d}/{len(examples)}] {name:<14s} ...", end="",
              flush=True)
        try:
            r = run_one(name, env, do_sc=not args.fast)
        except subprocess.TimeoutExpired:
            r = {"name": name, "builds": True, "runs": True, "sc_runs": False,
                 "sw_paint": 0, "sc_paint": 0, "tris": 0, "rmse": None,
                 "cycles": 0, "wall_ms": 60000, "note": "timeout"}
        rows.append(r)
        flag = " "
        if r["rmse"] is not None:
            flag = "✓" if r["rmse"] < args.rmse_max else "≈"
        elif r["sc_runs"]:
            flag = "?"
        elif r["runs"]:
            flag = "·"
        else:
            flag = "✗"
        rmse_s = f"{r['rmse']:.2f}" if r["rmse"] is not None else "—"
        print(f"  {flag} sw={r['sw_paint']:>6d} sc={r['sc_paint']:>6d}"
              f" rmse={rmse_s:>6s} ({r['note']})")

    # Markdown report.
    with open(args.out, "w") as f:
        f.write("# Example regression report\n\n")
        f.write(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write(f"Total examples: **{len(rows)}**\n")
        builds = sum(1 for r in rows if r["builds"])
        runs   = sum(1 for r in rows if r["runs"])
        scs    = sum(1 for r in rows if r["sc_runs"])
        match  = sum(1 for r in rows
                     if r["rmse"] is not None and r["rmse"] < args.rmse_max)
        f.write(f"| metric | count | % |\n")
        f.write(f"|---|---:|---:|\n")
        f.write(f"| builds  | {builds} | {100*builds/len(rows):.0f}% |\n")
        f.write(f"| runs    | {runs}   | {100*runs/len(rows):.0f}% |\n")
        if not args.fast:
            f.write(f"| sc-runs | {scs}    | {100*scs/len(rows):.0f}% |\n")
            f.write(f"| RMSE < {args.rmse_max} | {match} | {100*match/len(rows):.0f}% |\n")
        f.write("\n")

        f.write("Legend: ✓ match · ≈ rendered, RMSE high · ? rendered, dim mismatch · · sw_ref only · ✗ build/run fail\n\n")
        f.write("| | example | sw paint | SC paint | tris | cycles | RMSE | wall ms | note |\n")
        f.write("|---|---|---:|---:|---:|---:|---:|---:|---|\n")
        for r in rows:
            if r["rmse"] is not None:
                flag = "✓" if r["rmse"] < args.rmse_max else "≈"
            elif r["sc_runs"]:
                flag = "?"
            elif r["runs"]:
                flag = "·"
            else:
                flag = "✗"
            rmse_s = f"{r['rmse']:.2f}" if r["rmse"] is not None else "—"
            f.write(f"| {flag} | `{r['name']}` | {r['sw_paint']:,} "
                    f"| {r['sc_paint']:,} | {r['tris']} | {r['cycles']:,}"
                    f" | {rmse_s} | {r['wall_ms']} | {r['note']} |\n")

    print()
    print(f"report → {args.out}")
    print(f"builds  : {builds}/{len(rows)}")
    print(f"runs    : {runs}/{len(rows)}")
    if not args.fast:
        print(f"sc-runs : {scs}/{len(rows)}")
        print(f"matches : {match}/{len(rows)}  (RMSE < {args.rmse_max})")
    return 0 if (builds == len(rows) and runs == len(rows)) else 1


if __name__ == "__main__":
    sys.exit(main())
