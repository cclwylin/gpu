#!/usr/bin/env bash
# Nightly full regression.
set -euo pipefail

echo "[ci] nightly.sh: TBD (Phase 1 onward)"

# Planned:
#   1. Full shader corpus (~500) — compile + ISA sim == sw_ref
#   2. Full scene regression (~50) — systemc top == sw_ref
#   3. ES 2.0 CTS subset
#   4. dEQP MSAA subset (Phase 1+)
#   5. Coverage report -> dashboard
#   6. Perf metrics  -> dashboard
