#!/usr/bin/env python3
"""Aggregate the per-case TSV from `run_vkglcts_to_sc.py` into a
markdown summary by (group, status). Status buckets:
    bit-perfect  — RMSE == 0
    near-match   — 0 < RMSE <= --rmse-max
    diff         — RMSE > --rmse-max
    timeout      — sc_pattern_runner exceeded the timeout
    no-scene     — glcompat didn't capture anything
    other-error  — anything else (parse failures, sc rc != 0, etc.)
"""
from __future__ import annotations
import argparse
import sys
from collections import defaultdict
from pathlib import Path


def categorize(row: dict, rmse_max: float) -> str:
    note = row.get("note", "") or ""
    rmse = row.get("rmse", "")
    if "sc timeout" in note:
        return "timeout"
    if note == "no scene":
        return "no-scene"
    try:
        r = float(rmse)
    except ValueError:
        return "other-error" if note else "no-image"
    if r != r:  # NaN
        return "other-error" if note else "no-image"
    if r == 0.0:
        return "bit-perfect"
    if r <= rmse_max:
        return "near-match"
    return "diff"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("tsv", type=Path)
    ap.add_argument("--rmse-max", type=float, default=1.0)
    ap.add_argument("--out", type=Path, default=None,
                    help="Markdown file to write (default: stdout)")
    ap.add_argument("--top-level", action="store_true",
                    help="Group by the top-level CTS namespace only "
                         "(e.g. `fragment_ops` instead of "
                         "`fragment_ops.blend`). Compact summary form.")
    args = ap.parse_args()

    rows = []
    with open(args.tsv) as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            if not line.strip() or line.startswith("#"): continue
            parts = line.rstrip("\n").split("\t")
            row = dict(zip(header, parts))
            rows.append(row)

    if not rows:
        print(f"empty TSV: {args.tsv}", file=sys.stderr)
        return 1

    # group by either top-level (`fragment_ops`) or top-2
    # (`fragment_ops.blend`). Top-level is the user-facing one-line-
    # per-group form; top-2 keeps the per-subgroup spotlight.
    by_group: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    by_group_examples: dict[str, dict[str, str]] = defaultdict(dict)
    for row in rows:
        case = row["case"]
        parts = case.split(".")
        if args.top_level:
            grp = parts[0] if parts else case
        else:
            grp = ".".join(parts[:2]) if len(parts) >= 2 else case
        cat = categorize(row, args.rmse_max)
        by_group[grp][cat] += 1
        by_group[grp]["total"] += 1
        by_group_examples[grp].setdefault(cat, case)

    cats = ["bit-perfect", "near-match", "diff", "timeout", "no-scene",
            "other-error", "no-image"]

    # Compact form: one line per top-level group, columns sw_ref pass /
    # total + SC bit-perfect / diff / timeout / other.
    if args.top_level:
        sw_pass: dict[str, int] = defaultdict(int)
        sw_total: dict[str, int] = defaultdict(int)
        for row in rows:
            grp = row["case"].split(".")[0]
            sw_total[grp] += 1
            if row.get("status", "?") == "Pass":
                sw_pass[grp] += 1
        lines = []
        lines.append("# VK-GL-CTS sweep")
        lines.append("")
        lines.append(f"`{args.tsv}` — {len(rows)} cases through deqp-gles2 "
                     f"(sw_ref) and `sc_pattern_runner` (cycle-accurate SC chain).")
        lines.append("")
        lines.append("| group | sw_ref pass / total | SC bit-perfect | SC diff | SC timeout | SC error |")
        lines.append("|---|---:|---:|---:|---:|---:|")
        for grp in sorted(by_group):
            b = by_group[grp]
            total = b.get("total", 0)
            lines.append(
                f"| `{grp}` | {sw_pass[grp]} / {sw_total[grp]} | "
                f"{b.get('bit-perfect', 0)} | "
                f"{b.get('diff', 0) + b.get('near-match', 0)} | "
                f"{b.get('timeout', 0)} | "
                f"{b.get('no-scene', 0) + b.get('other-error', 0) + b.get('no-image', 0)} |"
            )
        # totals row
        total_sw_pass = sum(sw_pass.values())
        total_sw_total = sum(sw_total.values())
        total_bp = sum(b.get('bit-perfect', 0) for b in by_group.values())
        total_diff = sum(b.get('diff', 0) + b.get('near-match', 0) for b in by_group.values())
        total_to  = sum(b.get('timeout', 0) for b in by_group.values())
        total_err = sum(b.get('no-scene', 0) + b.get('other-error', 0) + b.get('no-image', 0)
                        for b in by_group.values())
        lines.append(f"| **total** | **{total_sw_pass} / {total_sw_total}** | "
                     f"**{total_bp}** | **{total_diff}** | **{total_to}** | **{total_err}** |")
        out = "\n".join(lines)
        if args.out:
            args.out.write_text(out + "\n")
            print(f"wrote {args.out}")
        else:
            print(out)
        return 0

    lines = []
    lines.append(f"# VK-GL-CTS → SC E2E summary")
    lines.append(f"")
    lines.append(f"`{args.tsv}` — {len(rows)} cases (rmse-max={args.rmse_max})")
    lines.append(f"")

    # totals
    totals: dict[str, int] = defaultdict(int)
    for grp_buckets in by_group.values():
        for cat, n in grp_buckets.items():
            totals[cat] += n
    grand = totals["total"]
    lines.append(f"## Totals")
    lines.append(f"")
    lines.append(f"| bucket | count | % |")
    lines.append(f"|---|---:|---:|")
    for cat in cats:
        n = totals.get(cat, 0)
        pct = (100.0 * n / grand) if grand else 0
        lines.append(f"| {cat} | {n} | {pct:.1f}% |")
    lines.append(f"| **total** | **{grand}** | 100.0% |")
    lines.append(f"")

    # per-group
    lines.append(f"## Per-group")
    lines.append(f"")
    lines.append(f"| group | total | bit-perfect | near-match | diff | timeout | no-scene | other |")
    lines.append(f"|---|---:|---:|---:|---:|---:|---:|---:|")
    for grp in sorted(by_group):
        b = by_group[grp]
        lines.append(
            f"| `{grp}` | {b['total']} | "
            f"{b.get('bit-perfect', 0)} | {b.get('near-match', 0)} | "
            f"{b.get('diff', 0)} | {b.get('timeout', 0)} | "
            f"{b.get('no-scene', 0)} | "
            f"{b.get('other-error', 0) + b.get('no-image', 0)} |"
        )

    # spotlight examples
    lines.append(f"")
    lines.append(f"## Spotlight (one example per non-empty cell)")
    lines.append(f"")
    for grp in sorted(by_group):
        ex = by_group_examples[grp]
        if not ex: continue
        lines.append(f"### `{grp}`")
        for cat in cats:
            if cat in ex:
                lines.append(f"- **{cat}**: `{ex[cat]}`")

    out = "\n".join(lines)
    if args.out:
        args.out.write_text(out + "\n")
        print(f"wrote {args.out}")
    else:
        print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
