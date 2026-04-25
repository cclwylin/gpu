---
block: L2
name: L2 Unified Cache
version: 0.1 (draft)
owner: E1
last_updated: 2026-04-25
---

# L2 — Unified Cache Microarchitecture

## Purpose

共享 cache,上游接 L1 Tex$ miss / MMU PTE fetch / CP ring fetch / TB list,
下游 AXI via MC。Coalesce small requests。

## Block Diagram

```
     L1 Tex$ miss    PTE fetch   CP fetch   TB list
          │              │           │          │
          ▼              ▼           ▼          ▼
     ┌────────────────────────────────────────────┐
     │            Request Arbiter                 │
     └──────┬─────────────────────────────────────┘
            ▼
     ┌────────────────────────────────────────────┐
     │  L2 Tag + Data  (8-way set assoc, 64 B)    │
     └──────┬─────────────────────────────────────┘
            │ miss
            ▼
     ┌────────────────────┐
     │   Miss Handler     │ (outstanding queue)
     └──────┬─────────────┘
            ▼
          MC (AXI)
```

## Config(v1,Phase 2 驗)

| 參數 | 值 |
|---|---|
| Size | 256 KB(Phase 2 驗 512 KB 效益) |
| Assoc | 8-way set associative |
| Line | 64 B |
| Policy | LRU |
| Write | write-through(簡化;Phase 0 決定是否 write-back) |

## Interface

| Port | Dir | Notes |
|---|---|---|
| `req_i[N]` | in | multi-client |
| `resp_o[N]` | out | line data / ack |
| `mc_req_o`, `mc_resp_i` | master | AXI via MC |
| `csr_*` | in | enable、flush、BIST |

## Clients & Bypass Policy

| Client | Cache |
|---|---|
| L1 Tex$ miss | Yes |
| MMU PTE | Yes(PTE 被多 client 共用) |
| CP ring fetch | Yes(streaming,但可能重讀) |
| TB list write | No(streaming write,bypass) |
| RSV write | No(streaming;already coalesce at RSV) |
| PFO/TBF | No(TBF 全 on-chip,不經 L2) |

## Outstanding Miss

- 最多 N outstanding(initial N=8)
- MSHR(Miss Status Holding Register):同 line 多次 miss 合併
- Coalesce:同一 line 的多 req 只觸發 1 次 refill

## BIST

- March on tag + data RAM
- 透過 CSR trigger

## Throughput

- Target 1 req / cycle at hit
- Miss path:dominant by DRAM latency(~50–100 cycle)

## Corner Cases

- Write / read 同 line concurrent:serialize
- MMU fault 途中 L2 req:drop + invalid resp
- Flush:walk all tag,invalidate(多 cycle,block new req)
- Cold start:所有 miss,warm-up cost

## Verification Plan

1. Hit / miss / conflict pattern
2. MSHR coalesce
3. Write-through correctness(若採用)
4. Flush during traffic
5. BIST

## Open Questions

- [ ] Write-through vs write-back(TBF bypass 已經省寫回,但 TB list / CP ring 可能 benefit from WB)
- [ ] Size 256 vs 512 KB
- [ ] MSHR 數(8 vs 16)
- [ ] Banking:若 frequency 不收斂,後期 bank 化
