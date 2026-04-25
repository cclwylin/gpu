#!/usr/bin/env bash
# Verilator lint for RTL (Phase 3+). Before RTL exists this is a no-op.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ ! -d rtl ] || [ -z "$(find rtl -name '*.v' -o -name '*.sv' 2>/dev/null)" ]; then
  echo "No RTL yet. Skipping Verilator lint."
  exit 0
fi

# Run lint on each top module directory
FAIL=0
for top in rtl/top/*.sv rtl/top/*.v; do
  [ -e "$top" ] || continue
  if ! verilator --lint-only -Wall "$top" -Irtl/blocks; then
    echo "::error file=$top::verilator lint failed"
    FAIL=1
  fi
done

exit $FAIL
