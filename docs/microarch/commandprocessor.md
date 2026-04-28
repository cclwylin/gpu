---
block: CP
name: Command Processor
version: 1.0 (frozen)
owner: E1
last_updated: 2026-04-26
---

# CP — Command Processor Microarchitecture

## Purpose

Parse driver-produced command ring buffer,維護 HW state,dispatch
draw / state-update / sync command 到下游 block。所有 HW 活動的起點。

## Implementation Status

- **Phase-1 LT** — `CommandProcessorLt` in [`systemc/blocks/commandprocessor/`](../../systemc/blocks/commandprocessor/). Shipped Sprint 5; Sprint 34 added multi-stage dispatch (`Stage::{VF,PA,RS,TMU,PFO}` + 5 initiator sockets + `enqueue(Stage,void*)` overload).
- **Phase-2 CA** — `CommandProcessorCa` (Sprint 18). `SC_CTHREAD` on `clk.pos()` + ready/valid + 64-bit data; same `enqueue()` driver-side API as LT.
- **Out of scope for v1**: ring-buffer fetch, prefetch FIFO, full scoreboard. The current LT model accepts a queued opaque `void*` and routes by `Stage` tag — the FSM described below maps to the Phase 2.x cycle-accurate buildout.

## Block Diagram

```
           APB CSR               AXI (ring fetch, via MMU)
             │                         │
             ▼                         ▼
      ┌─────────────┐           ┌──────────────┐
      │ CSR Shadow  │           │ Ring Fetcher │
      │ & IRQ logic │           │ + prefetch Q │
      └──────┬──────┘           └──────┬───────┘
             │                         │
             ▼                         ▼
      ┌────────────────────────────────────────┐
      │             FSM + Decoder              │
      │  IDLE ─► FETCH ─► DECODE ─► DISPATCH   │
      │                       │                │
      │                       ▼                │
      │              State Scoreboard          │
      └────────────────────┬───────────────────┘
                           │ dispatch
         ┌────────┬────────┼────────┬────────┐
         ▼        ▼        ▼        ▼        ▼
        VF      CSR      PMU      sync    errbus
        (draw) (state) (event)   (fence)
```

## Interface

| Port | Dir | Width | Notes |
|---|---|---|---|
| `clk`, `rst_n` | in | 1 | standard |
| `apb_*` | slave | 32 | CSR access |
| `axi_fb_m_*`(shared) | master | 128 | ring buffer fetch via MMU client |
| `vf_cmd_*` | out | TLM | draw packet to VF |
| `state_bus_*` | out | 64+ | state update fanout to各 block |
| `pmu_event_*` | out | event | to PMU |
| `irq_o` | out | 1 | done / error |

## State / Registers

見 [`specs/registers.yaml`](../../specs/registers.yaml) bank **CP**:
- `CP_CTRL`:enable / reset / halt
- `CP_RING_BASE / SIZE / HEAD / TAIL`
- `CP_STATUS`:idle / stall / err
- `CP_IRQ_STATUS`:RW1C

## Command Packet Format(v1)

```
 31     24 23      0
┌────────┬──────────┐
│ opcode │  length  │
└────────┴──────────┘
 (payload follows, length * 4 bytes)
```

### Opcodes(v0.1)
| Code | Name | Payload |
|---|---|---|
| 0x00 | NOP | — |
| 0x10 | SET_REG | bank_id(8) + offset(16) + value(32) * N |
| 0x11 | SET_REG_BULK | bank_id + offset + count + values |
| 0x20 | DRAW | primitive_mode + vertex_count + first + instance_count |
| 0x21 | DRAW_INDEXED | primitive_mode + index_count + index_base + ... |
| 0x30 | FENCE | fence_id |
| 0x31 | WAIT_IDLE | — |
| 0x40 | IRQ | irq_bit |

Reserved opcodes 保留 future extension,decoder 碰到 unknown → 進 `CP_STATUS.ERR`。

## FSM

```
IDLE ─ tail != head ─► FETCH
FETCH ─ packet valid ─► DECODE
DECODE ─ SET_REG ─► DISPATCH_STATE ─► IDLE
DECODE ─ DRAW    ─► WAIT_SB ─► DISPATCH_DRAW ─► IDLE
DECODE ─ FENCE   ─► DISPATCH_FENCE ─► IDLE
DECODE ─ WAIT_IDLE ─► WAIT_PIPELINE ─► IDLE
ANY ─ bad opcode ─► ERR ─► halt + IRQ
```

## Pipeline / Throughput

- Fetch 與 decode 可 overlap(prefetch Q depth = 4)
- Target avg:1 packet / 4 cycle
- Worst:DRAW with scoreboard stall(等 VF idle)

## State Scoreboard

追蹤 in-flight draw 數、state write 是否有 outstanding draw 仍引用舊 state。
Draw 必須等 scoreboard 許可才能 dispatch,防止 state aliasing。

## Corner Cases

- Ring buffer wrap:TAIL < HEAD 時的環繞處理
- Unaligned packet / truncated packet:latch err
- MMU fault during ring fetch:CP halt + IRQ
- Reset while in-flight:drain pipeline 後 reset

## Verification Plan

1. Directed:each opcode 獨立 exercise
2. Random:constrained-random packet generator
3. Formal:FSM reachability、deadlock freedom(SymbiYosys)
4. Corner:ring wrap、fault injection、reset mid-packet
5. Coverage:所有 transition、所有 opcode 都命中

## Open Questions

- [x] **Multi-stage dispatch** — Sprint 34 lands `Stage` enum + 5 initiator sockets in CP_lt; chip-level `gpu_top` binds them to PA / RS / TMU / PFO targets. Real ring-fetch + decoder is Phase 2.x.
- [ ] Prefetch depth:4 是否夠(workload-dependent) — Phase 2.x; deferred until ring-buffer fetch lands.
- [ ] Scoreboard 精度:per-register bank 或更粗 — Phase 2.x.
- [ ] Bulk SET_REG 對 APB throughput 影響 — deferred (no current workload exercises it).
