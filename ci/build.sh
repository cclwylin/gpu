#!/usr/bin/env bash
# Build everything (sw_ref + compiler + systemc + driver).
# Arg $1 = compiler (gcc|clang). Phase 0 placeholder.
set -euo pipefail

COMPILER="${1:-gcc}"
echo "[ci] build.sh with compiler=$COMPILER"

# Planned actions (Phase 0 end):
#   cmake -B build -DCMAKE_CXX_COMPILER=$COMPILER
#   cmake --build build -j$(nproc)
