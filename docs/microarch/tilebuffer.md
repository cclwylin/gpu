---
block: TBF
name: Tile Buffer
version: 1.0 (frozen)
owner: E2
last_updated: 2026-04-26
---

# TBF — Tile Buffer Microarchitecture

## Purpose

On-chip SRAM 存 per-sample color / depth / stencil。tile 結束時由 RSV 讀出
resolve 寫 DRAM。

## Implementation Status

- **Phase-1 LT** — `TileBufferLt` in [`systemc/blocks/tilebuffer/`](../../systemc/blocks/tilebuffer/) (Sprint 34). Storage placeholder: actual `color_samples` / `depth_samples` / `stencil_samples` live on the host as `Context::fb.*`. The LT block accepts a `TileFlushJob*` and forwards via initiator socket to RSV.
- **Phase-2 CA** — `TileBufferCa` (Sprint 26). Same storage model; the on-chip SRAM is modelled by host-side fb. Zero-cycle flush stamp (the upstream PFO already accounts for per-quad throughput).
- **Out of scope for v1**: 8-bank SRAM bank-conflict modeling, BIST, double-buffered tile pipelining, dedicated early-Z read port.

## Capacity

```
tile = 16 × 16 pixel
per-sample = 32b color + 24b depth + 8b stencil = 64 bit = 8 B
per-pixel × 4 sample = 32 B
per-tile = 256 pixel × 32 B = 8 KB (data)
```

**Physical size target:64 KB / tile**。原因:
- double-buffer(一 tile 在 render,另一 tile 在 resolve),+ overhead
- 可能 1× 模式下也保留空間
- Bank 劃分與 padding

Phase 2 決定最終 layout;初始 design 8 bank × 8 KB。

## Block Diagram

```
       PFO (per-sample RMW)
            │
            ▼
    ┌────────────────────┐
    │  Bank Arbiter      │ (8-way)
    └──┬──┬──┬──┬──┬──┬──┬──┬─
       │  │  │  │  │  │  │  │
       ▼  ▼  ▼  ▼  ▼  ▼  ▼  ▼
      B0 B1 B2 B3 B4 B5 B6 B7   (each 8 KB SRAM)
       │
       ▼
    ┌────────────────────┐
    │  Read Out Mux      │
    └──────┬─────────────┘
           ▼
         RSV
```

## Bank Allocation

策略:相鄰 pixel 分散到不同 bank,減少 quad (2×2) access conflict。

```
(bank) = ((pixel_x >> 1) ^ (pixel_y >> 1)) & 0x7
```
或 XOR-based hash(Phase 2 驗證)。

## Interface

| Port | Dir | Notes |
|---|---|---|
| `pfo_rw_*` | R/W | per-sample RMW,quad level |
| `rsv_rd_*` | read | tile flush time |
| `csr_*` | in | bank config、BIST ctrl |
| `bist_result_o` | out | BIST done / fail |

## Timing

- 1-port SRAM per bank;同 bank 的 R + W 要分 cycle
- Worst-case conflict(4 pixel of quad → 4 bank):可 1 cycle
- Worst(all 4 同 bank):4 cycle

## Initialization

- `glClear` on tile entry:broadcast clear value to all sample of all pixel
- Clear 由 RSV-like path 寫入(或 dedicated clear unit)

## Access Patterns

| Client | Access | Priority |
|---|---|---|
| PFO | per-sample RMW | high(render path) |
| RSV | read at flush | time-boxed(tile flush) |
| Early-Z read | read depth for coarse/fine Z | low / bypassed cache line? |

## BIST

- March test(C-/C+,SA 故障 coverage > 99%)
- 觸發透過 `TBF_CTRL.BIST_START`
- 結果寫 `TBF_STATUS`

## Corner Cases

- Tile list overflow → 分 phase render,TBF 內容跨 phase 保留(或重 clear?)
- Mid-tile MMU fault:render stall;TBF 狀態保留
- Power-on:需先 clear(BIST 會做一次全寫)

## Verification Plan

1. Bank conflict pattern coverage
2. RMW correctness(pipelined case)
3. BIST
4. Clear broadcast
5. Stress:random access 10M cycle no corruption

## Open Questions

- [ ] 64 KB 是否需要 double-buffer — Phase 2.x; current models are single-buffered (host vec).
- [ ] 8 bank 是否夠 — Phase 2.x; bank conflict modeling not implemented.
- [ ] SRAM 是否 splitting color/depth/stencil — Phase 2.x; `Framebuffer` keeps them as separate vectors today.
- [ ] Early-Z read 是否走 dedicated port — deferred until early-Z lands in PFO.
