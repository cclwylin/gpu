#!/usr/bin/env python3
"""End-to-end driver: VK-GL-CTS → glcompat → sw_ref → scene → SC chain.

For each requested CTS case:
  1. Run `deqp-gles2 --deqp-case=<name>` with `GLCOMPAT_SCENE=<scene>`
     so glcompat's atexit hook dumps the captured scene on exit.
  2. Pull the Result image from the qpa log → that's sw_ref's render.
  3. Run `sc_pattern_runner <scene> <ppm>` → SC chain's render.
  4. Diff SC PPM vs sw_ref PNG (per-pixel RMSE / max-err / diff-count).

Prints a markdown table; non-zero exit if any case's RMSE exceeds the
threshold (default 1.0, matching the project's existing SC-parity bar).

Usage:
    tools/run_vkglcts_to_sc.py --cases color_clear.single_rgb \\
                               --cases scissor.contained_tri \\
                               --rmse-max 1.0
    tools/run_vkglcts_to_sc.py --group color_clear --limit 5
"""
from __future__ import annotations

import argparse
import base64
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT       = Path(__file__).resolve().parents[1]
ARCH       = subprocess.run(["uname", "-m"], capture_output=True, text=True).stdout.strip()
BUILD      = ROOT / f"build-{ARCH}"
VKGLCTS_BIN = ROOT / f"build_vkglcts-{ARCH}" / "modules" / "gles2" / "deqp-gles2"
SC_BIN     = BUILD / "tests" / "conformance" / "sc_pattern_runner"
SC_LIBDIR  = "/usr/local/systemc-2.3.4/lib"


def parse_qpa_result_image(qpa: Path) -> bytes | None:
    """Return the first <Image Name="Result"> PNG bytes, or None."""
    text = qpa.read_text(errors="replace")
    m = re.search(
        r'<Image Name="Result"[^>]*>\s*([A-Za-z0-9+/=\s]+)\s*</Image>',
        text,
    )
    if not m:
        return None
    return base64.b64decode("".join(m.group(1).split()))


def parse_qpa_status(qpa: Path) -> str:
    text = qpa.read_text(errors="replace")
    m = re.search(r'<Result StatusCode="([^"]+)"', text)
    return m.group(1) if m else "?"


def read_ppm_p6(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    # Parse header — magic / w / h / maxval, each whitespace-separated.
    parts: list[bytes] = []
    p = 0
    while len(parts) < 4:
        # skip whitespace + comments
        while p < len(data) and data[p:p+1] in (b" ", b"\t", b"\n", b"\r"):
            p += 1
        if p < len(data) and data[p:p+1] == b"#":
            while p < len(data) and data[p:p+1] != b"\n":
                p += 1
            continue
        start = p
        while p < len(data) and data[p:p+1] not in (b" ", b"\t", b"\n", b"\r"):
            p += 1
        parts.append(data[start:p])
    magic, w, h, _maxv = parts
    if magic != b"P6":
        raise ValueError(f"not a P6 PPM: {path}")
    # skip a single whitespace after maxval
    if p < len(data) and data[p:p+1] in (b" ", b"\t", b"\n", b"\r"):
        p += 1
    W, H = int(w), int(h)
    pixels = []
    for i in range(W * H):
        off = p + i * 3
        pixels.append((data[off], data[off+1], data[off+2]))
    return W, H, pixels


def png_to_rgb(png_bytes: bytes) -> tuple[int, int, list[tuple[int, int, int]]]:
    """Use PIL to decode PNG → list of (R, G, B) tuples (alpha dropped)."""
    from io import BytesIO
    from PIL import Image
    im = Image.open(BytesIO(png_bytes)).convert("RGB")
    W, H = im.size
    return W, H, list(im.getdata())


def diff_rgb(a: list[tuple[int, int, int]],
             b: list[tuple[int, int, int]]) -> tuple[float, int, int]:
    """Returns (RMSE, max_err, diff_pixel_count)."""
    n = min(len(a), len(b))
    if n == 0:
        return (0.0, 0, 0)
    sq, mx, dp = 0, 0, 0
    for i in range(n):
        ar, ag, ab = a[i]
        br, bg, bb = b[i]
        dr, dg, db = ar - br, ag - bg, ab - bb
        e = max(abs(dr), abs(dg), abs(db))
        if e > 0: dp += 1
        if e > mx: mx = e
        sq += dr*dr + dg*dg + db*db
    rmse = math.sqrt(sq / (3 * n))
    return (rmse, mx, dp)


def run_one(case: str, tmp: Path, verbose: bool = False) -> dict:
    """Drive one CTS case end-to-end. Returns a row dict."""
    scene = tmp / f"{case}.scene"
    qpa   = tmp / f"{case}.qpa"
    sc_pp = tmp / f"{case}.sc.ppm"
    for f in (scene, qpa, sc_pp):
        f.unlink(missing_ok=True)

    # 1. deqp-gles2 with scene capture + image-log enabled.
    env = dict(os.environ)
    env["GLCOMPAT_SCENE"] = str(scene)
    cwd = VKGLCTS_BIN.parent
    cmd = [
        str(VKGLCTS_BIN),
        f"--deqp-case=dEQP-GLES2.functional.{case}",
        "--deqp-log-images=enable",
        "--deqp-log-shader-sources=disable",
        f"--deqp-log-filename={qpa}",
    ]
    deqp_rc = subprocess.run(cmd, cwd=str(cwd), env=env,
                             capture_output=not verbose, text=True).returncode
    row: dict = {
        "case": case,
        "deqp_rc": deqp_rc,
        "deqp_status": parse_qpa_status(qpa) if qpa.exists() else "?",
    }
    if not scene.exists():
        row["note"] = "no scene"
        return row

    # 2. SC chain.
    if not SC_BIN.is_file():
        row["note"] = "sc_pattern_runner missing"
        return row
    sc_env = dict(os.environ)
    sc_env["DYLD_LIBRARY_PATH"] = SC_LIBDIR + ":" + sc_env.get("DYLD_LIBRARY_PATH", "")
    try:
        rc = subprocess.run([str(SC_BIN), str(scene), str(sc_pp)],
                            env=sc_env, capture_output=True, text=True,
                            timeout=run_one._sc_timeout, check=False)
    except subprocess.TimeoutExpired:
        row["note"] = f"sc timeout (>{int(run_one._sc_timeout)}s)"
        return row
    if rc.returncode != 0 or not sc_pp.exists():
        row["note"] = f"sc fail (rc={rc.returncode})"
        return row
    for line in rc.stdout.splitlines():
        if "=" in line and not line.startswith(" "):
            k, v = line.split("=", 1)
            if k in ("CYCLES", "PAINTED", "TRIANGLES", "FLUSHES"):
                row[k.lower()] = int(v)

    # 3. Diff.
    png = parse_qpa_result_image(qpa)
    if png is None:
        row["note"] = "no Result image in qpa"
        return row
    sw_W, sw_H, sw = png_to_rgb(png)
    sc_W, sc_H, sc = read_ppm_p6(sc_pp)
    if (sw_W, sw_H) != (sc_W, sc_H):
        # The SC chain renders into the full glcompat fb (typically the
        # 256×256 deqp RT size); deqp only reads back the test's
        # viewport. Sprint 61 — try to recover the test's viewport
        # offset from the scene's `viewport` line so the crop lands on
        # the right sub-rect (otherwise blend tests render at a
        # vp_x≠0 viewport and the bottom-left crop misses the geometry).
        vp_x = vp_y = 0
        try:
            with open(scene) as fsf:
                for line in fsf:
                    parts = line.strip().split()
                    if len(parts) >= 5 and parts[0] == "viewport":
                        vp_x = int(parts[1]); vp_y = int(parts[2])
                        # First batch's viewport is enough — most tests
                        # use one viewport per render and the deqp
                        # Result image covers exactly that.
                        break
        except Exception:
            pass
        if sc_W >= sw_W + vp_x and sc_H >= sw_H + vp_y:
            cropped = []
            for y in range(sw_H):
                src_y = vp_y + y
                base  = src_y * sc_W + vp_x
                cropped.extend(sc[base : base + sw_W])
            sc = cropped
            row["note"] = f"cropped sc {sc_W}×{sc_H}→{sw_W}×{sw_H} @({vp_x},{vp_y})"
        else:
            row["note"] = f"size mismatch sw={sw_W}x{sw_H} sc={sc_W}x{sc_H}"
            return row
    rmse, mx, dp = diff_rgb(sw, sc)
    row["rmse"]    = rmse
    row["max_err"] = mx
    row["diff_px"] = dp
    return row


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", action="append", default=[],
                    help="Specific CTS case suffix (e.g. color_clear.single_rgb)")
    ap.add_argument("--cases-file", default=None,
                    help="One case-suffix per line (lines starting with # ignored)")
    ap.add_argument("--group", action="append", default=[],
                    help="Glob for `dEQP-GLES2.functional.<group>.*` (limited cases)")
    ap.add_argument("--limit", type=int, default=10,
                    help="When --group is used, cap this many cases")
    ap.add_argument("--sc-timeout", type=float, default=60.0,
                    help="Per-case sc_pattern_runner timeout in seconds")
    ap.add_argument("--workers", type=int, default=1,
                    help="Run this many cases in parallel (each case is "
                         "deqp + sc, both single-threaded)")
    ap.add_argument("--rmse-max", type=float, default=1.0,
                    help="Fail (exit 1) if any case's RMSE exceeds this")
    ap.add_argument("--tsv", default=None,
                    help="Write per-case results as TSV to this path")
    ap.add_argument("--resume", action="store_true",
                    help="Skip cases already present in the --tsv file")
    ap.add_argument("--keep-tmp", action="store_true",
                    help="Don't clean the /tmp scratch dir on exit")
    args = ap.parse_args()

    if not VKGLCTS_BIN.is_file():
        print(f"error: deqp-gles2 not built at {VKGLCTS_BIN}", file=sys.stderr)
        return 2
    if not SC_BIN.is_file():
        print(f"error: sc_pattern_runner not built at {SC_BIN}", file=sys.stderr)
        print("       configure with -DGPU_BUILD_SYSTEMC=ON and rebuild.",
              file=sys.stderr)
        return 2

    cases = list(args.cases)
    if args.cases_file:
        with open(args.cases_file) as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                cases.append(line)
    for grp in args.group:
        # List candidate cases via deqp's --deqp-runmode=stdout-caselist.
        rc = subprocess.run(
            [str(VKGLCTS_BIN),
             "--deqp-runmode=stdout-caselist",
             f"--deqp-case=dEQP-GLES2.functional.{grp}.*"],
            cwd=str(VKGLCTS_BIN.parent),
            capture_output=True, text=True, check=False, timeout=30)
        names = []
        for line in rc.stdout.splitlines():
            if line.startswith("TEST: "):
                full = line[len("TEST: "):].strip()
                # Strip the "dEQP-GLES2.functional." prefix.
                if full.startswith("dEQP-GLES2.functional."):
                    names.append(full[len("dEQP-GLES2.functional."):])
        if not names:
            print(f"warning: no cases listed for group {grp}", file=sys.stderr)
        cases.extend(names[: args.limit])
    if not cases:
        print("error: provide --cases <name> and/or --group <prefix>", file=sys.stderr)
        return 2

    run_one._sc_timeout = args.sc_timeout       # smuggle via fn attr.

    # Resume support: skip cases already present in TSV.
    done = set()
    tsv_fh = None
    if args.tsv:
        tsv_path = Path(args.tsv)
        if args.resume and tsv_path.exists():
            with open(tsv_path) as f:
                for line in f:
                    line = line.rstrip("\n")
                    if line and not line.startswith("#"):
                        done.add(line.split("\t", 1)[0])
            print(f"# resume: {len(done)} cases already in {tsv_path}", flush=True)
            tsv_fh = open(tsv_path, "a")
        else:
            tsv_fh = open(tsv_path, "w")
            tsv_fh.write("case\tstatus\trmse\tmax_err\tdiff_px\tsc_cycles\ttris\tnote\n")
            tsv_fh.flush()

    tmp = Path(tempfile.mkdtemp(prefix="cts2sc-"))
    print(f"# VK-GL-CTS → SC E2E sweep ({len(cases)} cases, sc-timeout={args.sc_timeout}s)",
          flush=True)
    print(f"# tmp dir: {tmp}\n")

    rows = []
    bad  = 0
    bit_perfect = 0
    pending = [c for c in cases if c not in done]
    import time, threading
    t_start = time.time()
    lock = threading.Lock()
    counter = {"idx": 0}

    def render_row(idx, case, row):
        nonlocal bad, bit_perfect
        rmse_s = f"{row.get('rmse', float('nan')):.3f}" if "rmse" in row else "—"
        ok = "rmse" in row and row["rmse"] <= args.rmse_max
        if "rmse" in row and row["rmse"] == 0.0: bit_perfect += 1
        if not ok and "rmse" in row: bad += 1
        if "note" in row and "cropped" not in row.get("note", ""): bad += 1
        elapsed = time.time() - t_start
        rate = idx / elapsed if elapsed > 0 else 0
        eta  = (len(pending) - idx) / rate if rate > 0 else 0
        print(f"[{idx:5d}/{len(pending)} {elapsed/60:5.1f}m, ETA {eta/60:4.1f}m, "
              f"bp={bit_perfect}, bad={bad}] {case}: "
              f"{row.get('deqp_status','?')} rmse={rmse_s} note={row.get('note','')}",
              flush=True)
        if tsv_fh:
            tsv_fh.write("\t".join([
                case, row.get("deqp_status", "?"),
                f"{row.get('rmse','nan')}",
                str(row.get("max_err", "")),
                str(row.get("diff_px", "")),
                str(row.get("cycles", "")),
                str(row.get("triangles", "")),
                row.get("note", ""),
            ]) + "\n")
            tsv_fh.flush()

    if args.workers <= 1:
        for case in pending:
            row = run_one(case, tmp)
            rows.append(row)
            counter["idx"] += 1
            render_row(counter["idx"], case, row)
    else:
        from concurrent.futures import ThreadPoolExecutor, as_completed
        # Each worker runs deqp + sc in its own subprocess; the GIL is
        # released across both via subprocess.run, so threads scale.
        # Per-worker scratch dirs avoid file collisions.
        worker_tmps = [tmp / f"w{i}" for i in range(args.workers)]
        for d in worker_tmps: d.mkdir()

        def submit(case, slot):
            return run_one(case, worker_tmps[slot])

        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            futs = {}
            slot_q = list(range(args.workers))
            iter_pending = iter(enumerate(pending, 1))
            in_flight = 0
            done_iter = False
            while not done_iter or in_flight:
                while slot_q and not done_iter:
                    try:
                        idx, case = next(iter_pending)
                    except StopIteration:
                        done_iter = True
                        break
                    slot = slot_q.pop()
                    fut = pool.submit(submit, case, slot)
                    futs[fut] = (idx, case, slot)
                    in_flight += 1
                if not in_flight: break
                done_set, _ = (lambda: ([fut for fut in list(futs)
                                        if fut.done()], None))()
                if not done_set:
                    time.sleep(0.05)
                    continue
                for fut in done_set:
                    idx, case, slot = futs.pop(fut)
                    row = fut.result()
                    rows.append(row)
                    in_flight -= 1
                    slot_q.append(slot)
                    with lock:
                        counter["idx"] += 1
                        render_row(counter["idx"], case, row)

    print()
    print(f"# {len(rows)} cases run "
          f"(bit-perfect={bit_perfect}, "
          f"diff-but-rendered={sum(1 for r in rows if 'rmse' in r and r['rmse']>0)}, "
          f"errors={sum(1 for r in rows if 'rmse' not in r and 'cropped' not in r.get('note',''))})")
    print(f"# {bad} over rmse-max={args.rmse_max} or with hard errors")
    if tsv_fh: tsv_fh.close()
    if not args.keep_tmp:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
