# compiler/assembler/ — Assembler / Disassembler

Shader text assembly ↔ binary round-trip。

## Input
- Assembly syntax defined in [`docs/isa_spec.md §7`](../../docs/isa_spec.md)
- ISA definition:[`specs/isa.yaml`](../../specs/isa.yaml)

## Generated
- `gen/` 由 `tools/isa_gen/` 從 `specs/isa.yaml` 生成的 opcode table、encoder、decoder skeleton。

## Tests
- Round-trip(assemble → disassemble → diff):100% pass for ISA corpus
- 手寫 3 reference shader 做為 Phase 0 ISA 表達力驗證
