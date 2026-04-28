---
block: RS
name: Rasterizer (MSAA-aware)
version: 1.0 (frozen)
owner: E2
last_updated: 2026-04-26
---

# RS — Rasterizer Microarchitecture

## Purpose

Render phase(per tile),對每個 triangle:
1. Setup edge equation
2. Coarse raster(tile-level quick reject)
3. Fine raster(per-pixel edge test + 4-sample MSAA coverage)
4. Barycentric → varying interpolation(送 SC 跑 FS)

## Implementation Status

- **Phase-1 LT** — `RasterizerLt` in [`systemc/blocks/rasterizer/`](../../systemc/blocks/rasterizer/) (Sprint 14). 1× and 4× MSAA paths, D3D rotated-grid pattern, perspective-correct varying via 1/w. Math copied from `sw_ref/src/pipeline/rasterizer.cpp`.
- **Phase-2 CA** — `RasterizerCa` (Sprint 22). Per-pixel-centre barycentric for varying / depth. 1 cyc/fragment placeholder; coarse-raster + per-tile binning are Phase 2.x.
- **sw_ref** — Sprint 3 baseline (1× + 4× MSAA, edge-fn). Sprint 17 added scissor (LT/CA still pending).
- **Out of scope for v1**: hierarchical-Z, scissor in CA path, points/lines (handled at PA expansion).

## Block Diagram

```
  Tile Render Scheduler
       │
       ▼ (tri from bin list)
   ┌──────────────────┐
   │   Setup Unit     │ (edge eq, barycentric derivs)
   └──────┬───────────┘
          ▼
   ┌──────────────────┐
   │  Coarse Raster   │ (tile accept / trivial reject)
   └──────┬───────────┘
          ▼
   ┌───────────────────────┐
   │  Fine Raster (per px) │
   │   ┌───────────────┐   │
   │   │ Edge Fn × 3   │──►│──► coverage mask (4 bit per pixel)
   │   │ × 4 sample    │   │
   │   └───────────────┘   │
   └──────────┬────────────┘
              ▼
     ┌─────────────────────────┐
     │ Barycentric / Varying   │
     │ Interpolation           │
     └──────────┬──────────────┘
                ▼
        SC (FS input: quad + mask + varying)
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `bin_list_i` | in | per-tile primitive descriptor |
| `sc_fs_in_*` | out | quad + mask + varying → SC |
| `csr_*` | in | MSAA enable、scissor、viewport |
| `mmu_*` | master | varying fetch(若存 DRAM) |

## Algorithm — Edge Function

經典 half-space edge function:
```
  E_i(x, y) = (y - y0) * (x1 - x0) - (x - x0) * (y1 - y0)
```
Triangle inside ⇔ 三個 edge 同號。

### Sub-pixel
- Screen-space 16.8 fixed-point(1/256 sub-pixel)
- Edge eq 用 integer 算,消除 floating-point accumulation drift

## Coverage(MSAA)

### 4 sample pattern(rotated-grid)
見 [`msaa_spec.md §2`](../msaa_spec.md)。

### Per pixel
- 對 4 個 sample 分別 eval edge function
- 4-bit coverage mask
- 1× mode:退化為 pixel-center eval,mask = 1 bit

### Implementation
- 4 份 edge-eval unit,或 1 份 4 cycle time-multiplexed
- **建議 4× parallel**(area vs timing)

## Quad Grouping

- Fragments 以 2×2 quad 送出(derivative 需求)
- Quad 包 4 pixel × 4 sample = 16 coverage bit
- `discard` / early-Z 仍 per-pixel,quad 結構只是 SC derivative 用途

## Barycentric / Varying Interp

- Setup 階段算 `dV/dx`、`dV/dy`(per varying per component)
- Fine raster 階段 incremental add
- Perspective correct:`V / w`、`1 / w` 先 interp 再 divide

## Coarse Raster

- Tile-level edge test:test tile 的 4 個 corner 是否同號
- 全 outside → reject,全 inside → full cover,部分 → 進 fine
- 減少 fine raster 工作量(edge-heavy 貢獻小)

## Early-Z (per sample)

- 在 RS 輸出前做 per-sample depth test(read TBF depth)
- 若所有 sample fail → skip FS(送 null quad)
- 若部分 fail → mask 更新後仍送 FS
- 若 FS 有 `discard` 或修改 depth → disable early-Z,走 late-Z in PFO

## Throughput

- Target:1 quad / cycle(4 pixel × 4 sample = 16 coverage eval / cycle)
- Large triangle 貢獻高 throughput;small triangle 被 setup latency 限制

## Corner Cases

- Zero-area triangle:reject in setup
- Pixel on edge:tie-break rule(top-left,D3D 傳統)
- Varying with `flat` qualifier(ES 2.0 沒有 `flat`,v1 不做)
- Scissor(rect reject,集成 coarse raster)

## Verification Plan

1. Triangle corner / edge coverage bit-exact vs sw_ref
2. MSAA coverage on thin triangle
3. Overlapping triangle order
4. Early-Z correctness(edge case:depth tie)
5. Large / small triangle 效率

## Open Questions

- [x] Setup unit:**FP** in v1 (matches sw_ref). Fixed-point pass is a Phase 2.x area optimisation.
- [x] Hier-Z / tile-level depth summary:**deferred** (v1 skips; coarse Z is bundled with coarse raster, not standalone).
- [ ] Coverage eval 4× parallel 還是 time-mux — Phase 2.x; CA model evaluates serially (1 cyc/fragment).
- [ ] Tie-break rule:**top-left** assumed (D3D / OpenGL agree); pixel-centre eval avoids edge-tie ambiguity in current tests, but a dedicated test scene would close this Q properly.
