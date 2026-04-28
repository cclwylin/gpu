---
block: PFO
name: Per-Fragment Ops
version: 1.0 (frozen)
owner: E2
last_updated: 2026-04-26
---

# PFO — Per-Fragment Ops Microarchitecture

## Purpose

FS 輸出 color 後的 per-sample 操作:
1. Alpha-to-coverage(若開啟)
2. Per-sample depth test
3. Per-sample stencil test
4. Per-sample blend
5. Write to TBF

## Implementation Status

- **Phase-1 LT** — `PerFragmentOpsLt` in [`systemc/blocks/perfragmentops/`](../../systemc/blocks/perfragmentops/) (Sprint 34). Wraps `gpu::pipeline::per_fragment_ops`.
- **Phase-2 CA** — `PerFragmentOpsCa` (Sprint 25). 4 cyc/quad placeholder.
- **sw_ref** — `gpu::pipeline::per_fragment_ops` covers depth + alpha blend (Sprint 8) and stencil + scissor + alpha-to-coverage (Sprint 17). Both 1× and 4× MSAA paths implemented.
- **Out of scope for v1**: 5-stage early-Z / late-Z pipeline timing, hi-Z block, blend pipe forwarding hazards, two-sided stencil, polygon offset, depth-bounds, `glColorMask`.

## Block Diagram

```
  SC (FS out: color + mask)
       │
       ▼
  ┌─────────────────┐
  │  A2C Unit       │ (mask = mask & a2c_mask(alpha))
  └──────┬──────────┘
         ▼
  ┌─────────────────────────────────┐
  │ Per-Sample Depth/Stencil Test   │  (4-lane parallel)
  │  read TBF.depth/stencil         │
  └──────┬──────────────────────────┘
         ▼ (mask 更新)
  ┌─────────────────────────────────┐
  │ Per-Sample Blend                 │  (4-lane parallel)
  │  read TBF.color, compute, write  │
  └──────┬──────────────────────────┘
         ▼
     TBF (RMW per sample)
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `sc_fs_out_i` | in | FS color + coverage mask |
| `tbf_*` | R/W | TBF per-sample access |
| `csr_*` | in | depth/stencil/blend/a2c state |
| `pmu_event_o` | out | a2c_hit, depth_fail, stencil_fail 等 |

## Alpha-to-Coverage

Algorithm 見 [`msaa_spec.md §5`](../msaa_spec.md)。

Hardware:
- 簡單 5-level LUT(基於 alpha 值)
- Output mask AND 原 mask

## Depth / Stencil Test

### Per-sample parallel
- 4 個 sample 同時進 test,4 份 compare unit
- Test order:stencil → depth(OpenGL 規定;stencil 可在 depth fail 時仍更新)

### Depth
- 24-bit depth,UNORM
- Func:LESS / LEQUAL / EQUAL / GEQUAL / GREATER / NOTEQUAL / ALWAYS / NEVER

### Stencil
- 8-bit stencil
- Func + mask + ref
- Ops on fail / zfail / zpass

### Update
- Pass → mask retain,update depth(若 writemask enabled)
- Fail → mask bit clear

## Blend

### Per-sample parallel
- 4 個 blend unit
- Each:read TBF.color、compute、write back

### Equation(ES 2.0)
- RGB / Alpha 分開
- Operand:ZERO / ONE / SRC_COLOR / ONE_MINUS_SRC_COLOR / DST_COLOR / ... / SRC_ALPHA / ...
- Op:FUNC_ADD / FUNC_SUBTRACT / FUNC_REVERSE_SUBTRACT

### Precision
- Input UNORM8
- Intermediate 16-bit fix(8.8 or 0.16)
- Output round + saturate → UNORM8

### Write mask
- Per-channel enable

## TBF Access

- Bank-aware(TBF 分 8 bank)
- 同 quad 的 4 pixel 盡量散布到不同 bank(避免 conflict)
- Outstanding request:pipelined,depth 4

## Throughput

- Target 1 quad / cycle(best case no bank conflict)
- Worst:bank-collided quad → 2 cycle

## Corner Cases

- All-masked pixel:skip PFO(FS 不該 emit)
- `discard` in FS → mask = 0 → skip
- Blend with dest alpha when no alpha buffer:treat as 1.0
- Depth test ALWAYS + write disable = fast path

## Verification Plan

1. Each blend equation / func combination
2. Stencil ops
3. A2C correctness(alpha sweep → mask monotonic)
4. Depth test at edge(sub-pixel accuracy)
5. Bank conflict stress:checkerboard pattern
6. Per-sample correctness:MSAA edge cases

## Open Questions

- [ ] Blend intermediate precision:0.16 vs 8.8 fix — Phase 2.x; sw_ref currently uses float internally.
- [ ] PFO 與 early-Z 重複邏輯是否共享硬體 — Phase 2.x; sw_ref does late-Z only.
- [x] Stencil back-face 獨立 state:**out of v1** (ES 2.0 没有 two-sided stencil).
- [x] Logic op(glLogicOp):**out of v1** (not in ES 2.0).
