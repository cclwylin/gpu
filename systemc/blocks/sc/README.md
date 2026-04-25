# SC — Shader Core

**Role**:Unified VS / FS,SIMT 16-thread warp。

## Interface
- In:VS → from VF;FS → from RS(fragment quad + coverage)
- Out:VS → PA;FS → PFO(color)
- TMU:request / response(texture sample)
- L1 I$:shader binary fetch(via MMU)

## Internal
- Warp scheduler(pick from multiple warp slots)
- 4 lane × vec4 ALU(per-lane predicate)
- Register file:32× vec4 GPR × 16 thread / warp slot(2 KB/slot)
- Constant buffer:16× vec4
- L1 I$
- Special function unit(rcp / rsq / exp / log / sin / cos,multi-cycle)
- TMU interface:request queue + response merger
- Varying load / store unit
- Execution mask stack(depth 8)

## Divergence
- Per-lane execution mask
- Structured CF 以 mask stack 管理
- Loop:loop stack depth 8

## Phase 1 strategy
- Wrap `compiler/isa_sim/`,只做 functional
- Phase 2 再做 pipeline 化

## Owner
E1
