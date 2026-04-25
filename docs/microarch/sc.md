---
block: SC
name: Shader Core
version: 0.1 (draft)
owner: E1
last_updated: 2026-04-25
---

# SC — Shader Core Microarchitecture

## Purpose

Unified SIMT shader core,執行 VS 和 FS。最複雜的 block。

## Block Diagram

```
   VS in (VF)      FS in (RS: quad + mask)
      │                  │
      └──────┬───────────┘
             ▼
     ┌────────────────┐
     │ Warp Packer    │ (合 16 thread)
     └────────┬───────┘
              ▼
       ┌───────────────────────────────────┐
       │   Warp Scoreboard  (N slots)      │
       │ ┌───────┬───────┬────...────┐     │
       │ │ slot0 │ slot1 │           │     │
       │ └───────┴───────┴────...────┘     │
       └──────────────┬────────────────────┘
                      ▼
              Warp Scheduler (round-robin + ready)
                      │
                      ▼
       ┌──── Fetch ──── Decode ────┬─────────────┐
       │                            │             │
       ▼                            ▼             ▼
    L1 I$                     ALU pipe        SFU pipe
                              (4 lane vec4)   (rcp/rsq/exp/log/sin/cos)
                                   │             │
                                   └──────┬──────┘
                                          ▼
                                    Register File
                                    (GPR + Const)
                                          │
                                          ▼
                                  TMU pipe   LSU pipe
                                      │        │
                              (TMU req)  (ld/st varying/uniform)

   Output:
     VS: varying + position  ─►  PA
     FS: color               ─►  PFO
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `vf_vs_in_*` | in | VS input vertex |
| `rs_fs_in_*` | in | FS input(quad + per-pixel mask) |
| `pa_vs_out_*` | out | VS varying + position |
| `pfo_fs_out_*` | out | FS color + mask |
| `tmu_req_* / tmu_resp_*` | req/resp | TMU interface |
| `mmu_i$_*` | master | shader binary fetch |
| `csr_*` | in | shader base addr、scratch、ubo |

## Warp Model

- Warp size 16 thread(4 lane × 4 thread batch)
- Warp slot 數:**N = 4**(initial;Phase 2 由 workload 驗證)
- Slot holds:
  - PC、execution mask、mask stack、loop stack
  - Per-thread GPR file(2 KB/slot)
  - Type tag(VS / FS)、metadata

## Pipeline

5-stage basic + variable-latency pipes:
```
  F  → D  → R  → EXEC → WB
                  │
                  ├── ALU  (1 cycle)
                  ├── SFU  (6–10 cycle pipelined)
                  ├── TMU  (20+ cycle, outstanding)
                  └── LSU  (10+ cycle)
```

Warp scheduler 每 cycle 從 ready warp 選一個。Hazard 透過 scoreboard。

## Register File

- Per-slot SRAM,2 KB(32 vec4 × 16 thread × 128 bit?)
  實際:32 vec4 × 128 bit × 16 thread = 64 KB / slot。→ **太大**。
- **Revised**:physical RF = total shared pool(e.g. 32 KB per slot × 4 slot = 128 KB),
  compiler-told GPR count,runtime allocate。**Phase 0 要決定分配策略**。

## Special Function Unit (SFU)

- Single pipelined unit,latency 6–10 cycle
- Ops:rcp / rsq / exp / log / sin / cos
- Tolerance:3 ULP(per ISA spec)
- Implementation:table lookup + polynomial refine

## L1 I$
- Direct-mapped,512 B,line = 64 B
- Shader binary 少(typical < 1 KB),hit ratio 應極高

## Divergence

- Per-lane execution mask(16-bit per warp)
- Mask stack(depth 8,per warp slot)
- `kil`:permanently mask off lane
- `loop` / `break` / `endloop`:loop stack 管理

## Output

- VS:`o0`(position)+ `o1..o7`(varying)
- FS:`o0`(color)
- `discard`:mask bit → 傳給 PFO,PFO 濾掉

## Corner Cases

- All-masked warp:immediate retire(no exec)
- Divergence 深度超過 stack:compiler 應拒絕 emit;HW 檢測 → error
- TMU response 與 warp 不匹配(TMU 回錯 tag):error
- Shader binary 跨 page 且 page fault:warp stall

## Verification Plan

1. Per-opcode directed test(assembly snippet)
2. Divergence torture:deeply nested if/else/loop
3. TMU flood:many outstanding tex
4. ISA conformance:跑 `tests/shader_corpus/`
5. Bit-exact vs `compiler/isa_sim/`:所有 shader

## Open Questions

- [ ] Warp slot 數 N:4 vs 8(area vs latency hiding)
- [ ] RF 分配:static vs dynamic occupancy
- [ ] SFU 是否 replicated(1 unit vs per-lane)
- [ ] Dual-issue(ALU + TMU 同 cycle)是否 v1
- [ ] Barrier / atomics:v1 不做(ES 2.0 沒有)
