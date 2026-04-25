# L2 — Unified Cache

**Role**:Shared cache for texture、framebuffer、command ring,coalesce small requests。

## Config(tentative,Phase 2 定)
- Size:256 KB(也考慮 512 KB;area vs miss rate 數據驅動)
- Organization:8-way set associative
- Line:64 B
- Policy:LRU

## Interface
- Upstream clients:L1 Tex$ miss、MMU walks、CP fetch、FB writes
- Downstream:MC(AXI master)

## Note
- TBF 不經過 L2(tile buffer 全 on-chip,tile 結束才出 DRAM)
- RSV 寫 DRAM 可以 bypass L2(streaming write;暫定策略 Phase 2 驗)

## Owner
E1
