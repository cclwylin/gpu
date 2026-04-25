---
block: MMU
name: Memory Management Unit
version: 0.1 (draft)
owner: E1
last_updated: 2026-04-25
---

# MMU — Memory Management Unit Microarchitecture

## Purpose

Virtual → physical translation,driver-managed page table,多 client 共享。

## Block Diagram

```
   Clients: VF, TMU, L2(fb), CP(ring), RSV, TB
      │  │  │  │  │  │
      ▼  ▼  ▼  ▼  ▼  ▼
    ┌────────────────────┐
    │   Client Arbiter   │ (RR)
    └──────┬─────────────┘
           ▼
    ┌────────────────────┐
    │      L1 TLB        │ (full-assoc, 16 entry)
    └──────┬─────────────┘
           │ miss
           ▼
    ┌────────────────────┐
    │      L2 TLB        │ (4-way, 256 entry)
    └──────┬─────────────┘
           │ miss
           ▼
    ┌────────────────────┐
    │  Page Table Walker │ (2-level)
    └──────┬─────────────┘
           │
           ▼
         L2 cache / MC (PTE fetch)

    Fault latch + IRQ    ◄─── walker miss / perm
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `client_req_i[N]` | in | per-client virt addr + type |
| `client_resp_o[N]` | out | phys addr or fault |
| `csr_*` | in | enable、PT base、flush |
| `irq_o` | out | fault |

## Page Format

- Page size:4 KB(only)
- Virtual addr:32-bit
- Physical addr:32-bit(v1,若需 > 4 GB 改 40-bit,Phase 0 定)

### 2-Level Page Table
```
VA[31:22] → L1 index(1024 entry,4 KB L1 table)
VA[21:12] → L2 index(1024 entry,4 KB L2 table per L1 entry)
VA[11:0]  → page offset
```

### PTE Format(32-bit)
```
 31                  12 11       2 1 0
┌───────────────────────┬──────────┬─┬─┐
│   physical page #     │  perm    │D│V│
└───────────────────────┴──────────┴─┴─┘
 V = valid
 D = dirty (unused in v1,留著)
 perm = RWX bits
```

## TLB

### L1(fully-associative,16 entry)
- Zero-cycle hit
- LRU replacement
- 每 entry 含:VA、PA、perm、valid

### L2(4-way set-associative,256 entry)
- 1-cycle hit
- LRU per-set

### Flush
- 全 flush via `MMU_CTRL.TLB_FLUSH`
- Per-ASID / per-addr flush:v1 不做(driver 重 load PT base 即可)

## Page Walker

- 單一 walker 實例(足夠,因 TLB hit 比例高)
- Walker miss outstanding:允許 1 in-flight,queue 後續 request
- Walker 讀 PTE 透過 L2 cache(PTE 會被 cache 加速)

## Fault Handling

三種 fault:
1. Translation fault:page not present
2. Permission fault:write to read-only,etc.
3. Walk error:PTE malformed

發生時:
- Latch `MMU_FAULT_ADDR` + `MMU_FAULT_STATUS`
- Block 該 client(送 fault response)
- IRQ 觸發

## Client Priority

Default round-robin。Phase 2 驗證是否需加權(e.g. VF 慢了整條 pipeline stall)。

## Throughput

- L1 hit:1 req / cycle
- L2 hit:1 req / cycle(pipelined)
- Miss:10–30 cycle(依 PTE cache hit)

## Corner Cases

- Page boundary crossing(e.g. texture block 跨 page):raise 2 request
- Concurrent flush 與 in-flight walk:walk abort、無 TLB update
- PT base change:強制全 flush
- Recursive PT(v1 不支援):walker 檢測 depth >2 → fault

## Verification Plan

1. Basic translation(hit / miss each level)
2. Flush behavior
3. Fault all three types
4. Concurrent clients
5. PT base change mid-op

## Open Questions

- [ ] Multiple walker(parallel miss handling)— v1 暫時 1
- [ ] Large page(2 MB)支援:v1 不做
- [ ] Per-ASID TLB tagging:v1 不做(single context)
