#!/usr/bin/env python3
"""Sprint 34 — pattern runner.

Runs a named .scene through both the sw_ref pipeline and (when
available) the SystemC cycle-accurate chain, dumps PPMs under
out/, and reports cycle / paint metrics.

Usage:
    python3 tools/run_pattern.py <pattern>          # default: build/
    python3 tools/run_pattern.py <pattern> --build-dir build-docker
    python3 tools/run_pattern.py --list             # list patterns

Patterns are resolved at tests/scenes/<name>.scene.

The SystemC binary (`sc_pattern_runner`) is Docker-only on
macOS+GCC due to the libstdc++/libc++ ABI mismatch in
/usr/local/systemc-2.3.4. The script detects whether the binary
got built and silently skips that half if not.
"""

from __future__ import annotations

import argparse
import math
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES_DIR = ROOT / "tests" / "scenes"
OUT_DIR = ROOT / "out"

KV_RE = re.compile(r"^([A-Z_]+)=(.+)$")


def list_patterns() -> list[str]:
    return sorted(p.stem for p in SCENES_DIR.glob("*.scene"))


def parse_kv(stdout: str) -> dict[str, str]:
    out = {}
    for line in stdout.splitlines():
        m = KV_RE.match(line.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    """Returns (W, H, raw_rgb_bytes)."""
    data = path.read_bytes()
    # Header: P6\n W H\n 255\n <binary>
    nl = 0
    parts = []
    cur = bytearray()
    while len(parts) < 3 and nl < len(data):
        b = data[nl]
        nl += 1
        if b in (0x0A, 0x20):    # newline or space
            if cur:
                parts.append(bytes(cur).decode())
                cur = bytearray()
        else:
            cur.append(b)
    if not parts or parts[0] != "P6":
        raise ValueError(f"{path}: not a P6 PPM")
    W, H = int(parts[1]), int(parts[2])
    # parts[3] would be 255 — already consumed by the second-newline.
    # Find end of header (after "255\n").
    head_end = data.index(b"255\n") + 4
    return W, H, data[head_end:]


def rmse(a: bytes, b: bytes) -> float:
    if len(a) != len(b):
        return float("inf")
    n = len(a)
    s = 0.0
    for i in range(0, n, 4096):
        chunk_a = a[i:i + 4096]
        chunk_b = b[i:i + 4096]
        for ca, cb in zip(chunk_a, chunk_b):
            d = ca - cb
            s += d * d
    return math.sqrt(s / n)


def run(cmd: list[str | Path]) -> subprocess.CompletedProcess:
    print("$", " ".join(str(c) for c in cmd))
    return subprocess.run(
        [str(c) for c in cmd], capture_output=True, text=True, check=False
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pattern", nargs="?")
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.list or args.pattern is None:
        print("patterns:")
        for p in list_patterns():
            print(f"  {p}")
        return 0

    scene_path = SCENES_DIR / f"{args.pattern}.scene"
    if not scene_path.is_file():
        print(f"unknown pattern: {args.pattern}", file=sys.stderr)
        print("available: " + ", ".join(list_patterns()), file=sys.stderr)
        return 2

    build_dir = ROOT / args.build_dir
    sw_bin = build_dir / "tests" / "conformance" / "scene_runner"
    sc_bin = build_dir / "tests" / "conformance" / "sc_pattern_runner"
    if not sw_bin.is_file():
        print(f"missing {sw_bin} — run cmake --build {args.build_dir} first",
              file=sys.stderr)
        return 2

    OUT_DIR.mkdir(exist_ok=True)
    sw_ppm = OUT_DIR / f"{args.pattern}.swref.ppm"
    sc_ppm = OUT_DIR / f"{args.pattern}.sc.ppm"

    # ---- sw_ref ----
    sw = run([sw_bin, scene_path, "--out", sw_ppm])
    if sw.returncode != 0:
        print(sw.stdout); print(sw.stderr, file=sys.stderr)
        return 1
    sw_kv = parse_kv(sw.stdout)
    print(sw.stdout.strip())

    # ---- SC chain (optional) ----
    sc_kv: dict[str, str] = {}
    if sc_bin.is_file():
        sc = run([sc_bin, scene_path, sc_ppm])
        if sc.returncode != 0:
            print(sc.stdout); print(sc.stderr, file=sys.stderr)
            return 1
        sc_kv = parse_kv(sc.stdout)
        print(sc.stdout.strip())
    else:
        print(f"(sc_pattern_runner not built at {sc_bin} — skipping CA chain;"
              " build inside Docker for SystemC)")

    # ---- Compare ----
    print()
    print("=" * 60)
    print(f"PATTERN: {args.pattern}")
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


if __name__ == "__main__":
    sys.exit(main())
