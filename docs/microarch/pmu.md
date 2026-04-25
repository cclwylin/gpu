---
block: PMU
name: Performance Monitor Unit
version: 0.1 (draft)
owner: E3
last_updated: 2026-04-25
---

# PMU — Performance Monitor Unit Microarchitecture

## Purpose

64 個 32-bit counter + trace buffer。Event 來自各 block,透過 mux 選源。

## Block Diagram

```
     Events from all blocks (CP/SC/RS/PFO/...)
              │
              ▼
       ┌──────────────────┐
       │ Event Bus Router │
       └──────┬───────────┘
              ▼
       ┌──────────────────────────────────────┐
       │ Counter 0  ... Counter 63            │
       │   each: event_sel(mux) + 32b count   │
       └──────┬───────────────────────────────┘
              ▼
       ┌──────────────────┐
       │  Trace Buffer    │ (circular SRAM, 32 KB)
       └──────┬───────────┘
              ▼
         DRAM / off-chip pin
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `event_bus_i[K]` | in | wide event vector from all block |
| `csr_*` | R/W | counter read、config write |
| `trace_axi_o` | out | trace dump to DRAM |
| `trace_pin_o` | out | off-chip trace(FPGA debug) |

## Counter

- 64 × 32-bit
- Per-counter:event_sel(mux index)+ saturate vs wrap 設定
- Read-reset option(read → auto clear)
- Overflow:IRQ optional

## Event Sources(示例)

| Source | Event IDs |
|---|---|
| CP | packet_cnt, draw_cnt, err, ring_empty_cycle |
| SC | warp_launch, inst_cnt, stall_cycle, divergence |
| RS | tri_cnt, coverage_hist_0..4, early_z_kill |
| PFO | depth_fail, stencil_fail, blend, a2c_hit |
| TMU | tex_req, l1_miss, block_decode |
| TBF | access, bank_conflict, spill |
| RSV | resolve_cycle, pixel_cnt |
| MMU | tlb_l1_miss, tlb_l2_miss, walk, fault |
| L2 | req, miss, mshr_full |
| MC | axi_tex_bytes_rd/wr, axi_fb_bytes_rd/wr, stall |

Event IDs Phase 0 末表列完整。

## Trace Buffer

### Format
- 32 KB circular SRAM
- Entry = 128 bit:timestamp(40) + block_id(8) + event_id(16) + payload(64)

### Trigger
- SW start / stop
- Event-triggered(watch for specific event → start)
- Periodic(every N cycle dump one sample)

### Dump Path
- On-chip SRAM → DRAM(via MC)
- 或 off-chip pin(serialized,低 bandwidth,for FPGA debug)

## MSAA-Specific Counters(highlight)

| Counter | Event |
|---|---|
| coverage_hist_0 | pixel with 0 sample covered |
| coverage_hist_1 | pixel with 1 sample covered |
| coverage_hist_2 | pixel with 2 sample covered |
| coverage_hist_3 | pixel with 3 sample covered |
| coverage_hist_4 | pixel with 4 sample covered |
| resolve_cycle | RSV active cycle |
| tbf_spill | TBF spill(unexpected;若 >0 為 bug) |
| a2c_hit | A2C modified mask count |
| early_z_per_sample_kill | per-sample early-Z kill count |

## Corner Cases

- Overflow:wrap vs saturate per-setting
- Reset:`PMU_CTRL.RESET` clear 所有 counter + trace ptr
- Concurrent read 與 increment:read returns sampled value(1 cycle sync)

## Verification Plan

1. Event count correctness(known workload)
2. Trace buffer wrap-around
3. Reset / start / stop sequencing
4. Overflow behavior

## Open Questions

- [ ] Event bus width(K bits):目前估 ~200 bit,集中 vs distributed
- [ ] Trace buffer size:32 KB 是否夠(FPGA debug 需要)
- [ ] Off-chip trace pin 數(面積 vs debug bandwidth)
