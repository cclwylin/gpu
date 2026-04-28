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

# `<x>/<y> Test  #<n>: <name> ............... Passed    0.03 sec`
# `<x>/<y> Test  #<n>: <name> ......***Failed   0.05 sec`
CTEST_RE = re.compile(
    r"^\s*\d+/\d+\s+Test\s+#\d+:\s+(\S+)\s+\.+(?:\s|\*+)*"
    r"(Passed|Failed|\*\*\*Failed|Not Run|Skipped|Timeout)",
)


def run_ctest_summary(build_dir: Path) -> list[dict]:
    """Run ctest in `build_dir`, return [{'name', 'status'}, ...].

    `status` is one of: passed / failed / skipped. The set of tests
    enumerated tracks whatever the project's CTestfile.cmake currently
    declares — so newly-added tests show up automatically without
    touching this script.
    """
    if not (build_dir / "CTestTestfile.cmake").exists():
        return []
    try:
        out = subprocess.run(
            ["ctest", "--no-tests=error"],
            cwd=str(build_dir),
            capture_output=True, text=True, timeout=600).stdout
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return []
    rows: list[dict] = []
    for line in out.splitlines():
        m = CTEST_RE.match(line)
        if not m:
            continue
        name, raw = m.group(1), m.group(2)
        status = ("passed"  if raw == "Passed" else
                  "skipped" if raw in ("Skipped", "Not Run") else
                  "failed")
        rows.append({"name": name, "status": status})
    return rows


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


def diff_metrics(a: bytes, b: bytes) -> tuple[float, int, int]:
    """Return (rmse, max_channel_err, diff_pixel_count) in one pass.

    rmse averages squared errors across all channels; max_channel_err
    surfaces the worst single-channel deviation; diff_pixel_count is
    the number of pixels where *any* channel disagrees. Together these
    distinguish "many pixels off slightly" (low RMSE, high diff_px,
    low max_err) from "few pixels off by a lot" (low RMSE, low diff_px,
    high max_err) — the original RMSE-only column hid both.
    """
    if len(a) != len(b):
        return float("inf"), 255, len(a) // 3
    sum_sq = 0
    max_err = 0
    diff_px = 0
    n_pix = len(a) // 3
    for i in range(n_pix):
        j = i * 3
        d0 = abs(a[j]     - b[j])
        d1 = abs(a[j + 1] - b[j + 1])
        d2 = abs(a[j + 2] - b[j + 2])
        m = d0 if d0 >= d1 and d0 >= d2 else (d1 if d1 >= d2 else d2)
        if m:
            diff_px += 1
            if m > max_err:
                max_err = m
        sum_sq += d0 * d0 + d1 * d1 + d2 * d2
    return math.sqrt(sum_sq / (n_pix * 3)), max_err, diff_px


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
        "pixels": 0,
        "sw_paint": 0,
        "sc_paint": 0,
        "tris": 0,
        "rmse": None,
        "max_err": None,
        "diff_px": None,
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
        row["pixels"] = W * H
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
            r, mx, dp = diff_metrics(sw_buf2, sc_buf)
            row["rmse"]    = r
            row["max_err"] = mx
            row["diff_px"] = dp
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
    ap.add_argument("--no-ctest", action="store_true",
                    help="skip the CTest-suites section at the bottom")
    args = ap.parse_args()

    OUT_DIR.mkdir(exist_ok=True)
    # Filter macOS AppleDouble shadows (`._foo.c`) that show up when this
    # tree is mounted on a non-HFS volume — same skip as the cmake globs.
    examples = sorted(p.stem for p in EX_DIR.glob("*.c")
                      if p.stem not in HELPERS and not p.stem.startswith("._"))
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
                 "pixels": 0, "sw_paint": 0, "sc_paint": 0, "tris": 0,
                 "rmse": None, "max_err": None, "diff_px": None,
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
        mx_s   = f"{r['max_err']:>3d}" if r["max_err"] is not None else "  —"
        dp_s   = f"{r['diff_px']:>5d}" if r["diff_px"] is not None else "    —"
        print(f"  {flag} sw={r['sw_paint']:>6d} sc={r['sc_paint']:>6d}"
              f" rmse={rmse_s:>6s} maxE={mx_s} diffPx={dp_s}"
              f" ({r['note']})")

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
        f.write("`maxE` = worst single-channel deviation (0..255). `diffPx` = "
                "pixels where any channel disagrees. Together they distinguish "
                "broad-but-shallow drift from a small spike.\n\n")
        f.write("| | example | pixels | sw paint | SC paint | tris | cycles | RMSE | maxE | diffPx | wall ms | note |\n")
        f.write("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n")
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
            mx_s   = f"{r['max_err']}" if r["max_err"] is not None else "—"
            dp_s   = f"{r['diff_px']:,}" if r["diff_px"] is not None else "—"
            f.write(f"| {flag} | `{r['name']}` | {r['pixels']:,} "
                    f"| {r['sw_paint']:,} | {r['sc_paint']:,}"
                    f" | {r['tris']} | {r['cycles']:,}"
                    f" | {rmse_s} | {mx_s} | {dp_s}"
                    f" | {r['wall_ms']} | {r['note']} |\n")

        # ----- CTest suites (everything declared in CTestTestfile.cmake) -----
        if not args.no_ctest:
            print("running ctest suites ...")
            ctest_rows = run_ctest_summary(BUILD_DIR)
            if ctest_rows:
                # Group by namespace prefix (compiler.* / sw_ref.* / ...).
                groups: dict[str, list[dict]] = {}
                for r in ctest_rows:
                    ns = r["name"].split(".", 1)[0] if "." in r["name"] else "(other)"
                    groups.setdefault(ns, []).append(r)

                total  = len(ctest_rows)
                passed = sum(1 for r in ctest_rows if r["status"] == "passed")
                f.write("\n## CTest suites\n\n")
                f.write(f"All registered ctest entries. Tracks `tests/`, `compiler/tests/`, "
                        f"`sw_ref/tests/`, `tests/conformance/`, `tests/glmark2_runner/`, and "
                        f"`systemc/tb/`. **{passed}/{total} passing.**\n\n")

                f.write("| namespace | passed | total |\n")
                f.write("|---|---:|---:|\n")
                for ns in sorted(groups):
                    p = sum(1 for r in groups[ns] if r["status"] == "passed")
                    f.write(f"| `{ns}.*` | {p} | {len(groups[ns])} |\n")

                f.write("\n<details><summary>Per-test detail</summary>\n\n")
                f.write("| status | test |\n|---|---|\n")
                glyph = {"passed": "✓", "failed": "✗", "skipped": "·"}
                for ns in sorted(groups):
                    for r in sorted(groups[ns], key=lambda r: r["name"]):
                        f.write(f"| {glyph.get(r['status'], '?')} | `{r['name']}` |\n")
                f.write("\n</details>\n")
            else:
                f.write("\n## CTest suites\n\n_(ctest not available or no tests declared)_\n")

        # ----- VK-GL-CTS sweep (Sprint 43+) -----
        # Sweep is generated separately by tools/run_vkglcts_sweep.py
        # because it depends on the deqp-gles2 binary built outside our
        # main build dir. If the snapshot exists, splice it in.
        sweep_path = OUT_DIR / "vkglcts_sweep.md"
        if sweep_path.is_file():
            f.write("\n## VK-GL-CTS sweep\n\n")
            f.write("Generated by `tools/run_vkglcts_sweep.py` against the "
                    "`gpu_glcompat` shim. See [docs/PROGRESS.md] Sprint 42-43 "
                    "for context. Re-run with `tools/run_vkglcts_sweep.py "
                    "--include-texture --include-shaders` for the heavier groups.\n\n")
            with open(sweep_path) as sweep:
                # Skip the sweep's own H1 (we're already inside another report).
                inside = False
                for ln in sweep:
                    if ln.startswith("# ") and not inside:
                        inside = True
                        continue
                    if inside:
                        f.write(ln)

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
