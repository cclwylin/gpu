---
block: VF
name: Vertex Fetch
version: 1.0 (frozen)
owner: E1
last_updated: 2026-04-26
---

# VF — Vertex Fetch Microarchitecture

## Purpose

根據 CP DRAW command:
1. 從 DRAM 讀 index buffer(`DRAW_INDEXED`)或產生 sequential index
2. 從 DRAM 讀 vertex attribute buffer
3. 做 format conversion(int / fixed / float / normalized)
4. 用 post-transform vertex cache 避開重複 VS 執行
5. 批次輸出 vertex 給 SC(VS input queue)

## Implementation Status

- **Phase-1 LT** — `VertexFetchLt` in [`systemc/blocks/vertexfetch/`](../../systemc/blocks/vertexfetch/). Sprint 10. Per-vertex `ShaderJob` fan-out via SC; format conversion, PT cache, MMU index-fetch are Phase 2.x.
- **Phase-2 CA** — `VertexFetchCa` (Sprint 19). Default `vertices_per_cmd = 3`; pass-through pointer downstream.
- **Out of scope for v1**: real index buffer + DRAM fetch, post-transform vertex cache, instancing.

## Block Diagram

```
    CP (DRAW cmd)
       │
       ▼
  ┌──────────┐      ┌────────────────┐
  │ Index Gen│─────►│  Index Fetcher │ (cache)
  │/Decoder  │      │  u8/u16/u32    │
  └──────────┘      └──────┬─────────┘
                           │ index
                           ▼
                   ┌──────────────────┐
                   │  PT-Vertex Cache │──hit──► SC (reuse)
                   │  (LRU, 32 entry) │
                   └──────┬───────────┘
                          │ miss
                          ▼
                   ┌──────────────────┐
                   │ Attribute Fetcher│
                   │  + format conv   │
                   └──────┬───────────┘
                          │
                          ▼
                      SC (VS input)
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `cp_cmd_i` | in | DRAW / DRAW_INDEXED packet |
| `mmu_*` | master | index + attribute fetch |
| `sc_vs_in_*` | out | vertex batch to shader core |
| `csr_*` | in | attribute binding config |

## State

Attribute binding(per-draw,driver-set via CSR):
- 最多 8 attribute streams
- Per-attr:base addr、stride、component count、format、offset

## Index Formats
- u8 / u16 / u32
- Restart index support(ES 2.0 不強制,v1 支援 u32 restart only;u8/u16 TBD)

## Format Conversion
| Format | Bits | Normalize |
|---|---|---|
| float32 | 32 | N/A |
| float16 | 16 | N/A(Phase 0 決定是否 v1 支援) |
| int16 | 16 | opt |
| int8 | 8 | opt |
| fixed | 32 | N/A |

Output 永遠 float32 vec4(短缺 component 補 0/0/0/1)。

## Caches

### Index cache
- Small,aimed at avoiding small burst thrashing
- Size:512 B,line = 64 B

### Post-transform vertex cache
- 32 entry,full varying + position
- LRU eviction
- Key = index value(hash)
- Target hit ratio > 50% on meshes with sharing

### Attribute cache
- Prefetch-friendly streaming
- Not strictly a cache(ring buffer),depth 每 attribute stream 獨立

## Throughput

- Target:2 vertex / cycle peak
- Limited by:DRAM BW(attribute-heavy meshes)或 SC input rate

## FSM

```
IDLE ─ cmd arrive ─► SETUP (binding fetch)
SETUP ─► FETCH_LOOP
FETCH_LOOP:
  for each index:
    cache hit? send to SC, continue
    miss? request attribute, on return format-convert, send to SC
FETCH_LOOP ─ all indices done ─► DONE ─► IDLE
```

## Corner Cases

- Zero-length draw:immediate DONE
- Out-of-range index:MMU fault latched in CP
- Attribute stream stride 0:broadcast 同一值(OpenGL 合法)
- Mid-draw state change:block in CP scoreboard(應不發生)

## Verification Plan

1. Each format 單獨 test + mixed
2. Large / small draw(1 vertex、1M vertex)
3. Cache thrashing 場景
4. Restart index
5. Attribute stride edge cases(0、negative 不合法 spec 外)

## Open Questions

- [ ] PT cache size 32 是否夠(workload 分析) — Phase 2.x; current LT/CA models bypass the cache.
- [x] float16 attribute — **deferred** out of v1 (ES 2.0 has no half-float vertex format).
- [x] Instance count support — **out of v1** (ES 2.0 lacks instancing; revisit if/when ES 3.0 / Vulkan target).
