# compiler/ — Shader Compiler Stack

GLSL(ES 2.0)→ SPIR-V → Custom SSA IR → Scheduled IR → Shader binary。

```
compiler/
  frontend/         GLSL → SPIR-V (via third_party/glslang)
  ir/               SSA IR definition + verifier
  passes/           const-fold / DCE / CSE / coalesce / swizzle / co-issue / schedule
  regalloc/         Linear-scan register allocator
  backend/          IR → ISA encoder, metadata writer
  assembler/        Text assembly ↔ binary (see assembler/)
  isa_sim/          ISA simulator (see isa_sim/)
```

## Key design decisions
- SPIR-V as IR entry:未來接 Vulkan / HLSL frontend 不重寫
- SSA + typed IR
- ABI 鎖在 [docs/isa_spec.md §6](../docs/isa_spec.md)
- Day-1 correctness,optimizer 分階段
- ISA 定義 source of truth:[specs/isa.yaml](../specs/isa.yaml)

## Deliverable
- Phase 1 exit:~500-shader corpus 全 compile,ISA sim 結果 = sw_ref model

## Owner
E3

## Subdirs
- [assembler/](assembler/)
- [isa_sim/](isa_sim/)
