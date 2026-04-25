# isa_gen

從 [`specs/isa.yaml`](../../specs/isa.yaml) 自動生成:

- `compiler/assembler/gen/opcodes.h` — Opcode enum + helpers
- `compiler/isa_sim/gen/dispatch.inc` — switch-case body for ISA simulator
- `systemc/common/gen/isa_decoder.h` — SystemC decoder constants
- `rtl/blocks/sc/gen/decoder.svh` — SV opcode defines
- `docs/gen/isa_reference.md` — 渲染後的 ISA 表

## Usage

```bash
python3 tools/isa_gen/isa_gen.py
```

亦會做:
- opcode 唯一性檢查(同 format 內 opcode 不可重複)
- 基本 schema sanity

## Dispatch example

`dispatch.inc` 須搭配 ISA simulator 的 switch block 使用:

```cpp
// executor.cpp
void execute(const Context& ctx, Inst inst) {
    switch (inst.op) {
    #include "compiler/isa_sim/gen/dispatch.inc"
    default: /* trap */ ;
    }
}
```

每個 `exec_<name>(ctx, inst)` 是人工實作(放在另一個 .cpp,非 generated)。
