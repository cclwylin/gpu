#!/usr/bin/env bash
# Check that all C++ / SystemC source files are clang-format clean.
# Exit non-zero if any file would be reformatted.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Find .h/.hpp/.cpp/.cc/.cxx in source dirs (skip third_party, generated, build)
FILES=$(find sw_ref compiler systemc rtl driver tools \
  -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \
          -o -name "*.h" -o -name "*.hpp" \) \
  -not -path "*/third_party/*" \
  -not -path "*/gen/*" \
  -not -path "*/build/*" 2>/dev/null || true)

if [ -z "$FILES" ]; then
  echo "No C++ source files yet. Skipping."
  exit 0
fi

FAIL=0
for f in $FILES; do
  if ! clang-format --dry-run --Werror "$f" 2>/dev/null; then
    echo "::error file=$f::clang-format violation"
    FAIL=1
  fi
done

exit $FAIL
