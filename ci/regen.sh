#!/usr/bin/env bash
# Regenerate codegen artefacts from specs/. CI then `git diff --exit-code`.
# Developer 忘記跑 → CI fail。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[ci] regen.sh"
python3 tools/regmap_gen/regmap_gen.py
python3 tools/isa_gen/isa_gen.py
