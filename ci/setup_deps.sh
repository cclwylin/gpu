#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Install build-time deps locally. Prefer docker/Dockerfile for reproducibility.
# 此腳本僅為 local dev convenience(已有 compiler / cmake / python 的 ubuntu)。
# -----------------------------------------------------------------------------
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[setup] Python deps..."
pip3 install --user -r third_party/requirements.txt

echo "[setup] Checking tool availability..."
for tool in clang-format clang-tidy verilator iverilog cmake python3; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "  [warn] $tool not found — consider using docker/Dockerfile instead"
  fi
done

echo "[setup] Checking SystemC..."
if [ -z "${SYSTEMC_HOME:-}" ] || [ ! -d "$SYSTEMC_HOME" ]; then
  echo "  [warn] SYSTEMC_HOME not set. Install via docker or build per third_party/versions.yaml."
fi

echo "[setup] Done. For full reproducibility:"
echo "  docker build -f docker/Dockerfile -t gpu-dev:v1 ."
