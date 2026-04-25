---
block: RSV
name: Resolve Unit
version: 0.1 (draft)
owner: E2
last_updated: 2026-04-25
---

# RSV — Resolve Unit Microarchitecture

## Purpose

Tile 結束時,讀 TBF 4 sample,box-filter 合成 1 pixel,寫出 DRAM framebuffer。
也處理 `glBlitFramebuffer`(MSAA → 1×)的 explicit resolve。

## Block Diagram

```
            TBF (read 4 sample per pixel)
                    │
                    ▼
          ┌─────────────────────┐
          │  Sample Reader      │ (4 parallel)
          └──────┬──────────────┘
                 ▼
          ┌─────────────────────┐
          │  Box Filter (avg)   │  (4→1 per channel)
          └──────┬──────────────┘
                 ▼
          ┌─────────────────────┐
          │  DRAM Write Buffer  │ (coalesce)
          └──────┬──────────────┘
                 ▼
                MMU → MC → AXI
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `tbf_rd_*` | read | 4-sample color read |
| `mmu_*` | master | DRAM write target |
| `csr_*` | in | FBO base、resolve mode |
| `trigger_i` | in | from tile scheduler or explicit blit cmd |
| `done_o` | out | signal to trigger next tile |

## Algorithm(box filter)

```
for each pixel in tile:
  for c in R, G, B, A:
    sum_c = s0[c] + s1[c] + s2[c] + s3[c]
    out_c = (sum_c + 2) >> 2              // round-to-nearest even
  write packed RGBA8 to DRAM
```

### Precision
- Input:4× UNORM8 per channel
- Sum:up to 10 bit(4 × 255)
- Output:8 bit after shift
- Round-to-nearest via +2(before shift)
- 沒有 dither(v1)

## Pipeline

3-stage:
```
  READ → FILTER → WRITE
```
Throughput target:1 pixel / cycle。

## DRAM Write

- Coalesce 4 × 8 B pixel → 32 B burst
- 實際 16 pixel × 4 B = 64 B burst(AXI 對齊)
- Unaligned start / end 特別處理(partial mask write)

## Resolve Modes

| Mode | Trigger | Destination |
|---|---|---|
| implicit | Tile flush | Driver-bound color RB(1× if MSAA FBO via GL_EXT) |
| explicit_blit | `glBlitFramebuffer` | User-specified dest FBO |
| none | 1× already | pass-through(no resolve,just write) |

## 1× Fast Path

若 MSAA 關閉(`FBO_CTRL.MSAA_EN = 0`):
- RSV 仍 active,單純當 tile buffer → DRAM writer
- 省 filter(4 sample 都同值)
- 或 TBF 只存 sample 0,RSV 直接 forward

Phase 2 決定:1× mode 是否 share 或 dedicated path。建議 share(簡化)。

## Corner Cases

- Tile 邊緣(screen 不整除 tile size):write mask partial
- FBO base 未對齊:MMU walk + partial burst
- Mid-resolve MMU fault:stall、IRQ
- `glBlitFramebuffer` 與 render concurrent:sequencing via CP sync

## Verification Plan

1. Box filter bit-exact golden(手算 +2 round)
2. Tile 邊界 write mask
3. Large tile flush(full 256 pixel)
4. Multiple tile back-to-back(no bubble)
5. `glBlitFramebuffer` full test(all tile at once)
6. 1× mode bypass

## Open Questions

- [ ] 1× fast path:share 還是 dedicated
- [ ] Resolve 時是否也 downsample depth / stencil(v1 不做 — depth resolve 語意不清)
- [ ] Filter 是否未來要支援 weighted(v1 固定 box)
- [ ] FBO base unaligned 支援範圍(8 byte 或 PAGE 對齊限制)
