#!/usr/bin/env bash
# Run ruff + black --check on Python sources.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PY_DIRS=(tools driver tests)
for d in "${PY_DIRS[@]}"; do
  [ -d "$d" ] || continue
  if find "$d" -name "*.py" -print -quit | grep -q .; then
    black --check "$d" || exit 1
    ruff check "$d"    || exit 1
  fi
done

echo "Python lint OK"
