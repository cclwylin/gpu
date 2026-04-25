#!/usr/bin/env bash
# Regenerate code from specs/. CI then asserts `git diff --exit-code`.
# If a developer forgot to run this after touching a spec, CI will fail.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[ci] regen.sh: regenerating from specs/"

# Placeholder until tools/regmap_gen and tools/isa_gen exist.
if [ -x tools/regmap_gen/regmap_gen.py ]; then
  python3 tools/regmap_gen/regmap_gen.py specs/registers.yaml
fi
if [ -x tools/isa_gen/isa_gen.py ]; then
  python3 tools/isa_gen/isa_gen.py specs/isa.yaml
fi
