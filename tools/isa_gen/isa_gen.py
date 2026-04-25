#!/usr/bin/env python3
"""
isa_gen — generate ISA artefacts from specs/isa.yaml.

Outputs:
  - compiler/assembler/gen/opcodes.h        (C++ opcode table)
  - compiler/isa_sim/gen/dispatch.inc       (C++ dispatch switch)
  - systemc/common/gen/isa_decoder.h        (SystemC decoder constants)
  - rtl/blocks/sc/gen/decoder.svh           (SV opcode constants)
  - docs/gen/isa_reference.md               (rendered markdown)

Usage:
  python3 tools/isa_gen/isa_gen.py [--spec specs/isa.yaml] [--root .]
"""

from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass, field

import yaml


# -----------------------------------------------------------------------------
# Data model
# -----------------------------------------------------------------------------
@dataclass
class Opcode:
    name: str
    format: str
    opcode: int         # the instruction opcode bits
    semantics: str = ""
    operands: int = 2

    @staticmethod
    def parse_opcode(v) -> int:
        """Accepts '0b000001' string or int. YAML parses 0b literals as int."""
        if isinstance(v, int):
            return v
        s = str(v).strip().replace("_", "")
        if s.startswith("0b"):
            return int(s, 2)
        if s.startswith("0x"):
            return int(s, 16)
        return int(s)


@dataclass
class IsaSpec:
    version: str
    params: dict
    opcodes: list[Opcode]
    fp: dict
    formats: dict = field(default_factory=dict)


# -----------------------------------------------------------------------------
# Loader
# -----------------------------------------------------------------------------
def load_spec(path: pathlib.Path) -> IsaSpec:
    data = yaml.safe_load(path.read_text())
    ops = []
    for o in data.get("opcodes", []):
        ops.append(Opcode(
            name=o["name"],
            format=o["format"],
            opcode=Opcode.parse_opcode(o["opcode"]),
            semantics=o.get("semantics", ""),
            operands=int(o.get("operands", 2)),
        ))
    # Sanity: opcode uniqueness
    seen = {}
    for op in ops:
        key = (op.format, op.opcode)
        if key in seen:
            raise ValueError(f"duplicate opcode in format {op.format}: "
                             f"{op.name} collides with {seen[key]}")
        seen[key] = op.name
    return IsaSpec(
        version=str(data["version"]),
        params=data.get("params", {}),
        opcodes=ops,
        fp=data.get("fp", {}),
        formats=data.get("formats", {}),
    )


# -----------------------------------------------------------------------------
# Emit helpers
# -----------------------------------------------------------------------------
AUTO_HEADER_CPP = (
    "// ---------------------------------------------------------------------\n"
    "// AUTO-GENERATED FROM specs/isa.yaml — DO NOT EDIT.\n"
    "// Regenerate with: python3 tools/isa_gen/isa_gen.py\n"
    "// ---------------------------------------------------------------------\n"
)


def _enum_name(op: Opcode) -> str:
    return f"OP_{op.name.upper()}"


# -----------------------------------------------------------------------------
# Emitters
# -----------------------------------------------------------------------------
def emit_opcodes_cpp(spec: IsaSpec) -> str:
    out = [AUTO_HEADER_CPP, "#pragma once\n#include <cstdint>\n\n",
           "namespace gpu::isa {\n\n",
           "enum class Opcode : uint32_t {\n"]
    for op in spec.opcodes:
        out.append(f"    {_enum_name(op)} = 0x{op.opcode:02X},  // {op.format}: {op.semantics}\n")
    out.append("};\n\n")

    # Name lookup
    out.append("inline const char* opcode_name(Opcode op) {\n")
    out.append("    switch (op) {\n")
    for op in spec.opcodes:
        out.append(f"        case Opcode::{_enum_name(op)}: return \"{op.name}\";\n")
    out.append("        default: return \"<unknown>\";\n    }\n}\n\n")

    # Format classifier
    out.append("enum class Format { ALU, FLOW, MEM, UNKNOWN };\n")
    out.append("inline Format opcode_format(Opcode op) {\n    switch (op) {\n")
    for op in spec.opcodes:
        fmt = op.format.upper()
        out.append(f"        case Opcode::{_enum_name(op)}: return Format::{fmt};\n")
    out.append("        default: return Format::UNKNOWN;\n    }\n}\n\n")

    # Operand count
    out.append("inline int opcode_operands(Opcode op) {\n    switch (op) {\n")
    for op in spec.opcodes:
        out.append(f"        case Opcode::{_enum_name(op)}: return {op.operands};\n")
    out.append("        default: return 0;\n    }\n}\n\n")

    out.append("}  // namespace gpu::isa\n")
    return "".join(out)


def emit_dispatch_inc(spec: IsaSpec) -> str:
    out = [AUTO_HEADER_CPP,
           "// Included by compiler/isa_sim/executor.cpp inside switch(op).\n\n"]
    for op in spec.opcodes:
        out.append(f"case Opcode::{_enum_name(op)}:\n"
                   f"    exec_{op.name}(ctx, inst);\n"
                   f"    break;\n")
    return "".join(out)


def emit_systemc_decoder(spec: IsaSpec) -> str:
    out = [AUTO_HEADER_CPP, "#pragma once\n#include <cstdint>\n\n",
           "namespace gpu::isa::decoder {\n\n"]
    for op in spec.opcodes:
        out.append(f"inline constexpr uint32_t OPC_{op.name.upper()} = 0x{op.opcode:02X};\n")
    out.append("\n}  // namespace gpu::isa::decoder\n")
    return "".join(out)


def emit_sv_decoder(spec: IsaSpec) -> str:
    out = ["// AUTO-GENERATED FROM specs/isa.yaml — DO NOT EDIT.\n",
           "`ifndef ISA_DECODER_SVH_\n`define ISA_DECODER_SVH_\n\n"]
    for op in spec.opcodes:
        out.append(f"`define OPC_{op.name.upper()}  6'h{op.opcode:02X}\n")
    out.append("\n`endif  // ISA_DECODER_SVH_\n")
    return "".join(out)


def emit_markdown(spec: IsaSpec) -> str:
    out = []
    out.append("<!-- AUTO-GENERATED — do not edit. Regenerate via tools/isa_gen/. -->\n\n")
    out.append(f"# ISA Reference (v{spec.version})\n\n")
    out.append("## Parameters\n\n")
    for k, v in spec.params.items():
        out.append(f"- `{k}` = {v}\n")
    out.append("\n## Floating point\n\n")
    for k, v in spec.fp.items():
        out.append(f"- `{k}`: {v}\n")
    out.append("\n## Opcodes\n\n")
    by_fmt: dict[str, list[Opcode]] = {}
    for op in spec.opcodes:
        by_fmt.setdefault(op.format, []).append(op)
    for fmt, ops in by_fmt.items():
        out.append(f"### Format: `{fmt}`\n\n")
        out.append("| Name | Opcode | Ops | Semantics |\n")
        out.append("|---|---|---|---|\n")
        for op in ops:
            out.append(
                f"| `{op.name}` | `0x{op.opcode:02X}` (0b{op.opcode:06b}) | "
                f"{op.operands} | {op.semantics or '—'} |\n"
            )
        out.append("\n")
    return "".join(out)


# -----------------------------------------------------------------------------
# Write
# -----------------------------------------------------------------------------
def write_if_changed(path: pathlib.Path, content: str) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text() == content:
        return False
    path.write_text(content)
    return True


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", default="specs/isa.yaml")
    ap.add_argument("--root", default=".")
    args = ap.parse_args()

    root = pathlib.Path(args.root).resolve()
    spec_path = root / args.spec
    if not spec_path.is_file():
        print(f"error: spec not found at {spec_path}", file=sys.stderr)
        return 2

    spec = load_spec(spec_path)

    artefacts = {
        root / "compiler/assembler/gen/opcodes.h":   emit_opcodes_cpp(spec),
        root / "compiler/isa_sim/gen/dispatch.inc":  emit_dispatch_inc(spec),
        root / "systemc/common/gen/isa_decoder.h":   emit_systemc_decoder(spec),
        root / "rtl/blocks/sc/gen/decoder.svh":      emit_sv_decoder(spec),
        root / "docs/gen/isa_reference.md":          emit_markdown(spec),
    }

    changed = 0
    for path, content in artefacts.items():
        if write_if_changed(path, content):
            changed += 1
            print(f"wrote {path.relative_to(root)}")
        else:
            print(f"unchanged {path.relative_to(root)}")
    print(f"\n{changed} file(s) updated.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
