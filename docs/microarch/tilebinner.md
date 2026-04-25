---
block: TB
name: Tile Binner
version: 0.1 (draft)
owner: E2
last_updated: 2026-04-25
---

# TB — Tile Binner Microarchitecture

## Purpose

TBDR 的核心:triangle 分到 tile bucket。每個 tile 在 DRAM 維護
primitive list,所有 draw 累積完成後進 render phase。

## Block Diagram

```
  PA (screen-space triangle)
        │
        ▼
  ┌──────────────┐
  │ BBox Compute │ (screen-space bounding box)
  └──────┬───────┘
         ▼
  ┌──────────────────┐
  │ Tile Range Iter  │ (iterate over overlapped tiles)
  └──────┬───────────┘
         ▼
  ┌──────────────────┐
  │ Per-Tile List    │ (DRAM writer,per-tile append)
  │ Write Buffer     │
  └──────┬───────────┘
         ▼
       MMU / MC
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `pa_tri_i` | in | screen-space triangle |
| `mmu_*` | master | per-tile list write |
| `csr_*` | in | bin list base addr / size config |
| `render_trigger_o` | out | 所有 draw 累積完,觸發 render phase |

## Binning Strategy

### Bounding-box based
- 算 triangle 的 screen BBox
- 遍歷覆蓋到的 tile,對每個 tile append triangle descriptor

### Triangle Descriptor(per entry in list)
```
  primitive_id:   24 bit
  state_ptr:      32 bit   (指向 CSR shadow state snapshot)
  edge_eqs:       3 × edge equation (or compute-on-demand?)
  varying_ptr:    32 bit
  packed flags:   8 bit
```

Phase 0 決定:descriptor 存 edge equation(空間大但省 RS 再算)
或只存 vertex index(小但 RS 要重算)。建議 **只存 vertex pointer + state**,
RS 重新 setup edge;面積小,setup unit 已經存在。

### Tile Grid
- Screen / 16 = tile count
- 範例:1920×1080 → 120 × 68 tile = 8160 tile
- Per-tile list 存在 DRAM 固定區(bin buffer)

### Bin Buffer Management
- 固定區:起點 `BIN_BASE`,size `BIN_SIZE`
- 每 tile 佔 fixed slot,不夠 chain 到 overflow pool
- Overflow 觸發 flush-and-retry(分批 render 同一 tile)

## FSM

```
IDLE ─ triangle arrive ─► BIN
BIN ─ triangle done ─► IDLE
ANY ─ end-of-frame ─► FLUSH ─ overflow? retry render loop
```

End-of-frame 由 CP 的 `WAIT_IDLE` 或 swap 觸發。

## Throughput

- Target avg:1 triangle / cycle for small-triangle workload
- Large triangle(covers many tile):limited by write BW

## Corner Cases

- Triangle 剛好跨 tile boundary:無論如何都寫兩 tile(conservative)
- Very large triangle(全螢幕):每 tile 都寫一份 → 可能塞爆 bin
- Bin overflow:render phase 分階段處理(per-render-pass)
- Zero draws in frame:skip render phase

## Verification Plan

1. Single triangle:check each tile 是否正確 enter
2. Grid of triangles:overlap correctness
3. Overflow scenario
4. Large triangle full-screen
5. End-of-frame flush

## Open Questions

- [ ] Descriptor 內容:存 edge eq 還是 vertex ptr
- [ ] Bin buffer 預設 size:per-tile quota 多少(pixel count 估)
- [ ] Overflow 策略:multi-pass render 或 on-demand enlarge
- [ ] Multiple-render-pass scheduler 的複雜度
