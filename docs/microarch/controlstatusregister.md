---
block: CSR
name: Configuration Register Block (APB slave)
version: 1.0 (frozen)
owner: E3
last_updated: 2026-04-26
---

# CSR — Register Block Microarchitecture

## Purpose

APB slave。Driver 透過 APB 讀寫 register,fanout config 給各 block。

## Implementation Status

- **Phase-1 LT** — `ControlStatusRegisterLt` in [`systemc/blocks/controlstatusregister/`](../../systemc/blocks/controlstatusregister/) (Sprint 34). 16 × 32-bit chip-internal register file; per `CsrJob`: read or write `regs_[reg_idx]`. The host-side bus protocol (APB / AHB) is **out of v1** — only the chip-internal handshake is modelled.
- **Phase-2 CA** — `ControlStatusRegisterCa` (Sprint 28). Same 16-reg file + 1-cyc access stamp.
- **regmap-gen** — Phase 0 already lands `tools/regmap_gen/` consuming `specs/registers.yaml`.

## Source of Truth

`specs/registers.yaml` → `tools/regmap_gen/` 自動生:
- `rtl/blocks/csr/gen/csr_regs.sv`(register storage + decode)
- `systemc/common/gen/regs.h`(offset constants)
- `driver/include/gen/gpu_regs.h`
- `docs/gen/register_map.md`

**Hand-written**:非 trivial 的 side-effect / cross-bank 依賴。

## Block Diagram

```
    APB slave
       │
       ▼
   ┌──────────────────┐
   │  Address Decode  │
   └──────┬───────────┘
          ▼
   ┌──────────────────┐
   │  Register File   │ (mostly generated)
   └──┬───────────────┘
      │
      ▼
   ┌──────────────────┐
   │  Fanout / Latch  │ (per-bank config_out)
   └──┬───────────────┘
      │
      ▼
    downstream blocks
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `apb_*` | slave | 32-bit data |
| `{block}_cfg_o[N]` | out | per-bank config bus |
| `{block}_status_i[N]` | in | status / counter snapshots |
| `irq_o` | out | aggregate of `CP_IRQ_STATUS`、`MMU_FAULT` 等 |

## Access Rules

| Access type | 實作 |
|---|---|
| RW | FF + read mux |
| RO | wire from status |
| RW1C | write-1-to-clear FF |
| WO | write-only,read returns 0 |

## Side-Effect Writes

Hand-coded cases(非 generated):
- `CP_RING_TAIL` write:trigger CP wake-up
- `TBF_CTRL.BIST_START` write:trigger BIST FSM
- `MMU_CTRL.TLB_FLUSH` write:pulse flush
- `PMU_CTRL.RESET`:clear all counter

## Clock Domain

- Single domain(per top-level 凍結)
- 若未來多 domain:APB domain 與 core domain 分開,加 sync

## Verification Plan

1. Protocol compliance(APB VIP)
2. Each register RW / reset value
3. Side-effect trigger
4. Back-to-back write(no state corruption)
5. Unaligned / reserved addr:return 0 / err

## Open Questions

- [x] Debug interface(JTAG):**out of v1**.
- [x] Register widening(64-bit counters):**deferred** — 32-bit suffices for v1. PMU cycle counter wraps after ~4.3 s @ 1 GHz, acceptable for current tests.
- [x] Sparse addr vs dense:**sparse** (decode table small).
