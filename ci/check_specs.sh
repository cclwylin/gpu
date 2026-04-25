#!/usr/bin/env bash
# Validate machine-readable specs (YAML syntactic + minimal schema).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

python3 - <<'PY'
import sys, yaml, pathlib

errors = 0

def check(path):
    global errors
    try:
        with open(path) as f:
            yaml.safe_load(f)
        print(f"OK   {path}")
    except yaml.YAMLError as e:
        print(f"FAIL {path}: {e}")
        errors += 1

check("specs/registers.yaml")
check("specs/isa.yaml")

# Minimal schema checks — extend in Phase 0.
regs = yaml.safe_load(open("specs/registers.yaml"))
assert "version" in regs,  "registers.yaml: missing 'version'"
assert "banks"   in regs,  "registers.yaml: missing 'banks'"
for b in regs["banks"]:
    assert "name" in b and "base" in b and "size" in b, \
        f"bank {b!r}: required fields missing"

isa = yaml.safe_load(open("specs/isa.yaml"))
assert "version" in isa and "opcodes" in isa, "isa.yaml malformed"

sys.exit(1 if errors else 0)
PY
