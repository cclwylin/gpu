---
block: MC
name: Memory Controller
version: 0.1 (draft)
owner: E1
last_updated: 2026-04-25
---

# MC — Memory Controller Microarchitecture

## Purpose

AXI4 master,request scheduling,QoS,兩條獨立 port 分離 texture / framebuffer 流量。

## Block Diagram

```
   L2 (read/write), RSV (write), TB (write)
        │
        ▼
   ┌──────────────────────┐
   │  Upstream Adapter    │ (classify: tex / fb)
   └──┬────────────┬──────┘
      │ tex path   │ fb path
      ▼            ▼
   ┌──────────┐  ┌──────────┐
   │ Scheduler│  │ Scheduler│
   └─┬────────┘  └─┬────────┘
     ▼             ▼
   AXI Master    AXI Master
   (axi_tex,128) (axi_fb,128)
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| Upstream AXI-like from L2 / RSV / TB | — | proprietary or simple AXI |
| `axi_tex_*` | AXI4 master | 128-bit |
| `axi_fb_*` | AXI4 master | 128-bit |
| `csr_*` | in | QoS weights |

## Classification

| Traffic | Port |
|---|---|
| L1 Tex$ refill | `axi_tex` |
| Uniform buffer fetch | `axi_tex` |
| CP ring fetch | `axi_tex` |
| MMU PTE walk | `axi_tex`(低量,共用 tex port) |
| TB primitive list | `axi_fb`(bulk write + later read) |
| RSV resolve write | `axi_fb` |
| FBO / tile list read | `axi_fb` |

2 port 讓讀 tex 與寫 fb 彼此不 block。

## Scheduling

Per-port:
- Read queue + write queue
- Arbiter:weighted round-robin
- Reordering:allowed 在同類別內(AXI OO 支援),跨 read/write 以 address 順序限制
- Burst:AXI4 burst length max 16 beat

## QoS

| Traffic | Priority |
|---|---|
| RSV write | high(tile flush 阻塞 pipeline) |
| L1 Tex$ refill | medium |
| TB list write | medium |
| CP ring fetch | low(latency-tolerant) |

Weights by CSR.

## Outstanding

- AWID / ARID 分流,允許 reordering
- Max outstanding per port:16(Phase 2 調)

## Corner Cases

- Write response order:AWID 相同時 strict order,不同 ID OK out-of-order
- Partial burst(tile 邊緣):WSTRB 處理
- AXI error(SLVERR / DECERR):latch 到 CSR,raise IRQ
- Back-pressure:upstream stall

## Verification Plan

1. AXI protocol compliance(VIP)
2. Burst boundary cross(4 KB)
3. Simultaneous two-port traffic
4. QoS:measure observed latency match setting
5. Error injection

## Open Questions

- [ ] 單 port 128-bit 是否足夠:peak BW 16 GB/s per port @ 1 GHz;FPGA 500 MHz → 8 GB/s
- [ ] DDR generation target(DDR4 vs LPDDR4):依 ASIC / FPGA 環境
- [ ] Write-combining buffer(coalesce 小寫)是否納 MC 還是 upstream
- [ ] AXI OO policy:保守(in-order)先做,後期放寬
