#!/usr/bin/env python3
"""
regmap_gen — generate register map artefacts from specs/registers.yaml.

Outputs:
  - driver/include/gen/gpu_regs.h       (C header)
  - systemc/common/gen/regs.h           (C++/SystemC header)
  - rtl/blocks/csr/gen/csr_regs.svh     (Verilog/SV header)
  - docs/gen/register_map.md            (rendered markdown)

Usage:
  python3 tools/regmap_gen/regmap_gen.py [--spec specs/registers.yaml] [--root .]
"""

from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass

import yaml


# -----------------------------------------------------------------------------
# Data model
# -----------------------------------------------------------------------------
@dataclass
class Field:
    name: str
    bits: str           # "5:0" or "3"
    desc: str = ""

    @property
    def msb(self) -> int:
        return int(self.bits.split(":")[0])

    @property
    def lsb(self) -> int:
        s = self.bits.split(":")
        return int(s[1]) if len(s) == 2 else int(s[0])

    @property
    def width(self) -> int:
        return self.msb - self.lsb + 1

    @property
    def mask(self) -> int:
        return ((1 << self.width) - 1) << self.lsb


@dataclass
class Register:
    name: str
    offset: int
    access: str         # RW, RO, RW1C, WO
    reset: int = 0
    fields: list[Field] = None

    def __post_init__(self):
        if self.fields is None:
            self.fields = []


@dataclass
class Bank:
    name: str
    base: int
    size: int
    description: str
    registers: list[Register]


@dataclass
class Spec:
    version: str
    address_base: int
    address_size: int
    bus: str
    data_width: int
    banks: list[Bank]


# -----------------------------------------------------------------------------
# Loader
# -----------------------------------------------------------------------------
def load_spec(path: pathlib.Path) -> Spec:
    data = yaml.safe_load(path.read_text())
    addr = data["address_space"]
    banks = []
    for b in data["banks"]:
        regs = []
        for r in b.get("registers", []):
            fields = [Field(name=f["name"], bits=f["bits"], desc=f.get("desc", ""))
                      for f in r.get("fields", [])]
            regs.append(Register(
                name=r["name"],
                offset=int(r["offset"]),
                access=r.get("access", "RW"),
                reset=int(r.get("reset", 0)),
                fields=fields,
            ))
        banks.append(Bank(
            name=b["name"],
            base=int(b["base"]),
            size=int(b["size"]),
            description=b.get("description", ""),
            registers=regs,
        ))
    return Spec(
        version=str(data["version"]),
        address_base=int(addr["base"]),
        address_size=int(addr["size"]),
        bus=addr["bus"],
        data_width=int(addr["data_width"]),
        banks=banks,
    )


# -----------------------------------------------------------------------------
# Emit helpers
# -----------------------------------------------------------------------------
AUTO_HEADER = (
    "// ---------------------------------------------------------------------\n"
    "// AUTO-GENERATED FROM specs/registers.yaml — DO NOT EDIT.\n"
    "// Regenerate with: python3 tools/regmap_gen/regmap_gen.py\n"
    "// ---------------------------------------------------------------------\n"
)


def _abs_offset(bank: Bank, reg: Register) -> int:
    return bank.base + reg.offset


# -----------------------------------------------------------------------------
# Emitters
# -----------------------------------------------------------------------------
def emit_c_header(spec: Spec) -> str:
    out = [AUTO_HEADER, "#ifndef GPU_REGS_H_\n#define GPU_REGS_H_\n\n",
           "#include <stdint.h>\n\n"]
    for bank in spec.banks:
        out.append(f"/* ==== Bank {bank.name} : {bank.description} ==== */\n")
        out.append(f"#define GPU_{bank.name}_BASE 0x{bank.base:08X}u\n")
        out.append(f"#define GPU_{bank.name}_SIZE 0x{bank.size:08X}u\n\n")
        for reg in bank.registers:
            abs_off = _abs_offset(bank, reg)
            out.append(f"#define GPU_REG_{reg.name}           0x{abs_off:08X}u  "
                       f"/* {reg.access}, reset=0x{reg.reset:08X} */\n")
            out.append(f"#define GPU_REG_{reg.name}_RESET     0x{reg.reset:08X}u\n")
            for f in reg.fields:
                out.append(
                    f"#define GPU_{reg.name}_{f.name}_LSB    {f.lsb}u\n"
                    f"#define GPU_{reg.name}_{f.name}_WIDTH  {f.width}u\n"
                    f"#define GPU_{reg.name}_{f.name}_MASK   0x{f.mask:08X}u\n"
                )
            out.append("\n")
    out.append("#endif /* GPU_REGS_H_ */\n")
    return "".join(out)


def emit_systemc_header(spec: Spec) -> str:
    out = [AUTO_HEADER, "#pragma once\n#include <cstdint>\n\n",
           "namespace gpu::regs {\n\n"]
    for bank in spec.banks:
        out.append(f"// Bank {bank.name}: {bank.description}\n")
        out.append(f"inline constexpr uint32_t {bank.name}_BASE = 0x{bank.base:08X};\n")
        out.append(f"inline constexpr uint32_t {bank.name}_SIZE = 0x{bank.size:08X};\n\n")
        for reg in bank.registers:
            abs_off = _abs_offset(bank, reg)
            out.append(f"inline constexpr uint32_t {reg.name}       = 0x{abs_off:08X};\n")
            out.append(f"inline constexpr uint32_t {reg.name}_RESET = 0x{reg.reset:08X};\n")
            for f in reg.fields:
                out.append(
                    f"inline constexpr uint32_t {reg.name}_{f.name}_LSB   = {f.lsb};\n"
                    f"inline constexpr uint32_t {reg.name}_{f.name}_WIDTH = {f.width};\n"
                    f"inline constexpr uint32_t {reg.name}_{f.name}_MASK  = 0x{f.mask:08X};\n"
                )
            out.append("\n")
    out.append("}  // namespace gpu::regs\n")
    return "".join(out)


def emit_sv_header(spec: Spec) -> str:
    out = [AUTO_HEADER.replace("//", "//"), "`ifndef CSR_REGS_SVH_\n`define CSR_REGS_SVH_\n\n"]
    for bank in spec.banks:
        out.append(f"// Bank {bank.name}\n")
        out.append(f"`define {bank.name}_BASE 32'h{bank.base:08X}\n")
        for reg in bank.registers:
            abs_off = _abs_offset(bank, reg)
            out.append(f"`define {reg.name}_ADDR   32'h{abs_off:08X}\n")
            out.append(f"`define {reg.name}_RESET  32'h{reg.reset:08X}\n")
            for f in reg.fields:
                out.append(
                    f"`define {reg.name}_{f.name}_MSB   {f.msb}\n"
                    f"`define {reg.name}_{f.name}_LSB   {f.lsb}\n"
                )
        out.append("\n")
    out.append("`endif  // CSR_REGS_SVH_\n")
    return "".join(out)


def emit_markdown(spec: Spec) -> str:
    out = []
    out.append("<!-- AUTO-GENERATED — do not edit. Regenerate via tools/regmap_gen/. -->\n\n")
    out.append(f"# Register Map (v{spec.version})\n\n")
    out.append(f"- Address base: `0x{spec.address_base:08X}`\n")
    out.append(f"- Address size: `0x{spec.address_size:08X}`\n")
    out.append(f"- Bus: `{spec.bus}` / data width: `{spec.data_width}`\n\n")
    for bank in spec.banks:
        out.append(f"## Bank `{bank.name}` — {bank.description}\n\n")
        out.append(f"- Base: `0x{bank.base:08X}`, Size: `0x{bank.size:08X}`\n\n")
        out.append("| Offset | Name | Access | Reset | Fields |\n")
        out.append("|---|---|---|---|---|\n")
        for reg in bank.registers:
            fields_str = ", ".join(
                f"`{f.name}`[{f.bits}]" for f in reg.fields
            ) or "—"
            out.append(
                f"| `0x{_abs_offset(bank, reg):08X}` | `{reg.name}` | {reg.access} | "
                f"`0x{reg.reset:08X}` | {fields_str} |\n"
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
    ap.add_argument("--spec", default="specs/registers.yaml")
    ap.add_argument("--root", default=".")
    args = ap.parse_args()

    root = pathlib.Path(args.root).resolve()
    spec_path = root / args.spec
    if not spec_path.is_file():
        print(f"error: spec not found at {spec_path}", file=sys.stderr)
        return 2

    spec = load_spec(spec_path)

    artefacts = {
        root / "driver/include/gen/gpu_regs.h":     emit_c_header(spec),
        root / "systemc/common/gen/regs.h":         emit_systemc_header(spec),
        root / "rtl/blocks/csr/gen/csr_regs.svh":   emit_sv_header(spec),
        root / "docs/gen/register_map.md":          emit_markdown(spec),
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
