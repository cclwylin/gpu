---
block: CSR
name: Configuration Register Block (APB slave)
version: 0.1 (draft)
owner: E3
last_updated: 2026-04-25
---

# CSR — Register Block Microarchitecture

## Purpose

APB slave。Driver 透過 APB 讀寫 register,fanout config 給各 block。

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

- [ ] Debug interface(JTAG)是否 v1 必備
- [ ] Register widening(64-bit)for certain counters
- [ ] Sparse addr vs dense(目前稀疏,decode 表較小)
