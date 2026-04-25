#!/usr/bin/env python3
"""Sprint 34/36 — pattern runner.

Two modes, picked from the pattern name:

  * `<scene>` (e.g. `triangle_white`)
        Resolves at tests/scenes/<name>.scene.
        Runs sw_ref's scene_runner; runs the SystemC sc_pattern_runner
        if its binary exists. Dumps PPMs to out/<name>.swref.ppm and
        out/<name>.sc.ppm and prints metrics + RMSE.

  * `examples/<name>` (e.g. `examples/cube`)
        Resolves at tests/examples/<name>.c.
        Builds the glcompat-linked binary `glex_<name>` and runs it,
        which compiles GL 1.x → renders through sw_ref → dumps a PPM.
        Doesn't go through the SC chain (the .c is too dynamic to
        capture as a static .scene yet).

Usage:
    python3 tools/run_pattern.py <pattern>
    python3 tools/run_pattern.py --list
    python3 tools/run_pattern.py <pattern> --build-dir build-docker
"""

from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES_DIR = ROOT / "tests" / "scenes"
EXAMPLES_DIR = ROOT / "tests" / "examples"
OUT_DIR = ROOT / "out"

KV_RE = re.compile(r"^([A-Z_]+)=(.+)$")


def list_scenes() -> list[str]:
    return sorted(p.stem for p in SCENES_DIR.glob("*.scene"))


def list_examples() -> list[str]:
    skip = {"logo", "mjkimage", "trackball"}     # helper-only files
    return sorted(p.stem for p in EXAMPLES_DIR.glob("*.c") if p.stem not in skip)


def parse_kv(stdout: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in stdout.splitlines():
        m = KV_RE.match(line.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    nl, parts, cur = 0, [], bytearray()
    while len(parts) < 3 and nl < len(data):
        b = data[nl]; nl += 1
        if b in (0x0A, 0x20):
            if cur:
                parts.append(bytes(cur).decode())
                cur = bytearray()
        else:
            cur.append(b)
    if not parts or parts[0] != "P6":
        raise ValueError(f"{path}: not a P6 PPM")
    W, H = int(parts[1]), int(parts[2])
    head_end = data.index(b"255\n") + 4
    return W, H, data[head_end:]


def rmse(a: bytes, b: bytes) -> float:
    if len(a) != len(b):
        return float("inf")
    s = 0.0
    for ca, cb in zip(a, b):
        d = ca - cb
        s += d * d
    return math.sqrt(s / len(a))


def painted_count(buf: bytes) -> int:
    return sum(1 for i in range(0, len(buf) - 2, 3) if buf[i] | buf[i+1] | buf[i+2])


def run(cmd: list, env: dict | None = None) -> subprocess.CompletedProcess:
    print("$", " ".join(str(c) for c in cmd))
    return subprocess.run([str(c) for c in cmd],
                          capture_output=True, text=True, check=False,
                          env=env)


def run_scene(args, scene_path: Path) -> int:
    build_dir = ROOT / args.build_dir
    sw_bin = build_dir / "tests" / "conformance" / "scene_runner"
    sc_bin = build_dir / "tests" / "conformance" / "sc_pattern_runner"
    if not sw_bin.is_file():
        print(f"missing {sw_bin} — run cmake --build {args.build_dir} first",
              file=sys.stderr)
        return 2
    OUT_DIR.mkdir(exist_ok=True)
    name = scene_path.stem
    sw_ppm = OUT_DIR / f"{name}.swref.ppm"
    sc_ppm = OUT_DIR / f"{name}.sc.ppm"

    sw = run([sw_bin, scene_path, "--out", sw_ppm])
    if sw.returncode != 0:
        print(sw.stdout); print(sw.stderr, file=sys.stderr)
        return 1
    sw_kv = parse_kv(sw.stdout)
    print(sw.stdout.strip())

    sc_kv: dict[str, str] = {}
    if sc_bin.is_file():
        sc = run([sc_bin, scene_path, sc_ppm])
        if sc.returncode != 0:
            print(sc.stdout); print(sc.stderr, file=sys.stderr)
            return 1
        sc_kv = parse_kv(sc.stdout)
        print(sc.stdout.strip())
    else:
        print(f"(sc_pattern_runner not built at {sc_bin} — SC chain skipped)")

    print()
    print("=" * 60)
    print(f"PATTERN: {name}")
    print(f"  TRIANGLES   : {sw_kv.get('TRIANGLES', '?')}")
    print(f"  sw_ref PPM  : {sw_ppm}")
    print(f"  sw_ref paint: {sw_kv.get('PAINTED', '?')}")
    if sc_kv:
        sw_W, sw_H, sw_buf = read_ppm(sw_ppm)
        sc_W, sc_H, sc_buf = read_ppm(sc_ppm)
        diff = rmse(sw_buf, sc_buf) if (sw_W, sw_H) == (sc_W, sc_H) else float("inf")
        cycles = int(sc_kv.get("CYCLES", "0"))
        flushes = int(sc_kv.get("FLUSHES", "0"))
        painted = int(sc_kv.get("PAINTED", "0"))
        cyc_per_pix = cycles / painted if painted else 0.0
        print(f"  SC PPM      : {sc_ppm}")
        print(f"  SC paint    : {painted}")
        print(f"  SC cycles   : {cycles:,}")
        print(f"  SC flushes  : {flushes}")
        print(f"  cyc / pixel : {cyc_per_pix:.1f}")
        print(f"  RMSE swref↔sc: {diff:.3f}")
    print("=" * 60)
    return 0


def run_example(args, c_path: Path) -> int:
    """Build glex_<name>, execute it (sw_ref render → PPM + .scene),
    then run sc_pattern_runner on the captured .scene if available."""
    build_dir = ROOT / args.build_dir
    name = c_path.stem
    OUT_DIR.mkdir(exist_ok=True)
    sw_ppm    = OUT_DIR / f"glex_{name}.ppm"
    scene_out = OUT_DIR / f"glex_{name}.scene"
    sc_ppm    = OUT_DIR / f"glex_{name}.sc.ppm"

    # 1. cmake build the target.
    bin_path = build_dir / "glcompat" / f"glex_{name}"
    print(f"$ cmake --build {args.build_dir} --target glex_{name}")
    rc = subprocess.run(
        ["cmake", "--build", str(build_dir),
         "--target", f"glex_{name}", "-j", "4"],
        capture_output=True, text=True, check=False)
    if rc.returncode != 0 or not bin_path.is_file():
        print("BUILD FAILED:")
        for line in (rc.stderr or rc.stdout).splitlines():
            if "error:" in line:
                print("   ", line); break
        return 1

    # 2. Run the binary — sw_ref render + capture .scene.
    import os
    env = dict(os.environ)
    env["GLCOMPAT_OUT"]   = str(sw_ppm)
    env["GLCOMPAT_SCENE"] = str(scene_out)
    rc = run([bin_path], env=env)
    if rc.returncode != 0:
        print(rc.stdout); print(rc.stderr, file=sys.stderr); return 1
    print(rc.stderr.strip())

    # 3. SC chain via sc_pattern_runner. The TBF/RSV placeholders cost
    # O(tile_w × tile_h) cycles per flush, so we cap fb at 64×64 for
    # the SC pass by default — keeps wall-clock under 10 s on most
    # examples. Override with --sc-fb-max or env SC_FB_MAX.
    sc_bin = build_dir / "tests" / "conformance" / "sc_pattern_runner"
    sc_kv: dict[str, str] = {}
    if sc_bin.is_file() and scene_out.is_file():
        env.setdefault("SC_FB_MAX", str(args.sc_fb_max))
        rc = run([sc_bin, scene_out, sc_ppm], env=env)
        if rc.returncode != 0:
            print("SC chain FAILED:")
            print(rc.stdout); print(rc.stderr, file=sys.stderr)
        else:
            sc_kv = parse_kv(rc.stdout)
            print(rc.stdout.strip())
    else:
        if not sc_bin.is_file():
            print(f"(sc_pattern_runner not built — SC chain skipped)")

    # 4. Summarise.
    if not sw_ppm.is_file():
        print("(no PPM written)"); return 1
    W, H, sw_buf = read_ppm(sw_ppm)
    sw_paint = painted_count(sw_buf)
    print()
    print("=" * 60)
    print(f"EXAMPLE: {name}.c")
    print(f"  source        : {c_path}")
    print(f"  sw_ref PPM    : {sw_ppm}  ({W}×{H})")
    print(f"  sw_ref painted: {sw_paint}  ({100.0*sw_paint/(W*H):.1f}%)")
    if scene_out.is_file():
        ntri = sum(1 for ln in scene_out.read_text().splitlines()
                   if ln and ln[0] in " \t-0.123456789")
        print(f"  scene capture : {scene_out}  ({ntri // 3} triangles)")
    if sc_kv:
        sw_W, sw_H, sw_buf2 = read_ppm(sw_ppm)
        sc_W, sc_H, sc_buf  = read_ppm(sc_ppm)
        cycles  = int(sc_kv.get("CYCLES",  "0"))
        flushes = int(sc_kv.get("FLUSHES", "0"))
        sc_paint = int(sc_kv.get("PAINTED", "0"))
        cyc_per_pix = cycles / sc_paint if sc_paint else 0.0
        print(f"  SC PPM        : {sc_ppm}  ({sc_W}×{sc_H})")
        print(f"  SC painted    : {sc_paint}  ({100.0*sc_paint/(sc_W*sc_H):.1f}%)")
        print(f"  SC cycles     : {cycles:,}")
        print(f"  SC flushes    : {flushes}")
        print(f"  cyc / pixel   : {cyc_per_pix:.1f}")
        if (sw_W, sw_H) == (sc_W, sc_H):
            diff = rmse(sw_buf2, sc_buf)
            print(f"  RMSE swref↔sc : {diff:.3f}")
        else:
            print(f"  (size mismatch sw {sw_W}×{sw_H} vs sc {sc_W}×{sc_H} — "
                  "RMSE skipped; run with --sc-fb-max ≥ {} to compare)"
                  .format(max(sw_W, sw_H)))
    print("=" * 60)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pattern", nargs="?")
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--sc-fb-max", type=int, default=64,
                    help="Cap SC-chain framebuffer size per side (default 64; "
                         "TBF/RSV cycle placeholders scale with tile area)")
    args = ap.parse_args()

    if args.list or args.pattern is None:
        print("scenes (run via sw_ref + SC chain):")
        for p in list_scenes():
            print(f"  {p}")
        print()
        print("examples (compiled GL 1.x → sw_ref):")
        for p in list_examples():
            print(f"  examples/{p}")
        return 0

    if args.pattern.startswith("examples/"):
        name = args.pattern.split("/", 1)[1]
        c_path = EXAMPLES_DIR / f"{name}.c"
        if not c_path.is_file():
            print(f"unknown example: {name}", file=sys.stderr); return 2
        return run_example(args, c_path)

    scene_path = SCENES_DIR / f"{args.pattern}.scene"
    if not scene_path.is_file():
        print(f"unknown pattern: {args.pattern}", file=sys.stderr)
        print("see --list", file=sys.stderr)
        return 2
    return run_scene(args, scene_path)


if __name__ == "__main__":
    sys.exit(main())
