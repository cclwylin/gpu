---
block: TMU
name: Texture Unit
version: 1.0 (frozen)
owner: E2
last_updated: 2026-04-26
---

# TMU — Texture Unit Microarchitecture

## Purpose

對 SC 發出的 texture 請求:address gen、L1 Tex$ lookup、format decode、filter,
回填 filtered texel。

## Implementation Status

- **Phase-1 LT** — `TextureUnitLt` in [`systemc/blocks/textureunit/`](../../systemc/blocks/textureunit/) (Sprint 34). Wraps `gpu::sample_texture`; per-request fill of `samples[i]`.
- **Phase-2 CA** — `TextureUnitCa` (Sprint 24). 1 cyc/request placeholder.
- **sw_ref** — `gpu::sample_texture` in `sw_ref/src/texture/` (Sprint 4). RGBA8 only; NEAREST + BILINEAR; CLAMP + REPEAT.
- **Out of scope for v1**: L1 Tex$ tag-check pipeline, mipmap LOD selection, RGB565 / ETC1 decoders, MIRRORED_REPEAT, anisotropic filter.

## Block Diagram

```
  SC (tex req: binding + uv + mode)
       │
       ▼
  ┌────────────────────────┐
  │  Binding Lookup (CSR)  │
  └──────┬─────────────────┘
         ▼
  ┌────────────────────────┐
  │  LOD Compute (from dFdx/dFdy) + level select
  └──────┬─────────────────┘
         ▼
  ┌────────────────────────┐
  │  Address Gen (u,v → texel addr × 4 for bilinear)
  └──────┬─────────────────┘
         ▼
  ┌────────────────────────┐
  │    L1 Tex$ (direct-map, 4×4 block) │
  │    miss → L2            │
  └──────┬─────────────────┘
         ▼
  ┌────────────────────────┐
  │ Format Decoder (RGBA8 / RGB565 / ETC1)
  └──────┬─────────────────┘
         ▼
  ┌────────────────────────┐
  │ Bilinear Filter (4-tap)│
  └──────┬─────────────────┘
         ▼
       SC (filtered texel)
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `sc_tex_req_i` | in | binding + uv + mode |
| `sc_tex_resp_o` | out | RGBA32 filtered texel |
| `mmu_*` | master | L1 miss path(via L2) |
| `csr_*` | in | binding slot config(16 slot) |

## LOD Computation

```
  dUdx = (du/dx_pixel) * tex_width
  dVdx = ...
  dUdy = ...
  dVdy = ...
  rho = max(sqrt(dUdx² + dVdx²), sqrt(dUdy² + dVdy²))
  lambda = log2(rho) + bias
  LOD = clamp(lambda, 0, max_level)
```

`log2` 用近似 LUT + polynomial,tolerance 1/16 step。

### Mipmap filter
- NEAREST:pick LOD level
- LINEAR:LOD lerp(trilinear = 2× bilinear + lerp)

## L1 Tex$

- Direct-mapped
- Block = **4×4 texel**(better 2D locality than linear)
- Size:8 KB(Phase 0 調整)
- Miss path:fetch block from L2
- Compressed formats(ETC1):store compressed 於 L1,decode on read(省空間)

## Format Decoder

### RGBA8 / RGB565
- 單 cycle decode
- RGB565 → RGBA8 擴展

### ETC1
- 4×4 block compression,64-bit/block
- Decode LUT + diff table
- Multi-cycle decode OK(因為 block 會被 4×4 個 texel access 攤提)

## Bilinear Filter

4-tap:
```
  tex(u, v) =
    (1-fu)(1-fv) * T00 +
    fu*(1-fv)   * T10 +
    (1-fu)*fv   * T01 +
    fu*fv       * T11
```
Per-channel,8-bit fix / 8-bit fix → 16-bit intermediate → 8-bit。

## Wrap Mode

- CLAMP_TO_EDGE
- REPEAT(uv mod size)
- MIRRORED_REPEAT

在 address gen 前套用。

## Throughput

- Target 1 filtered texel / cycle(with cache hit)
- Miss path:20 cycle avg(L2 hit)
- Pipelined:可多個 outstanding req

## Corner Cases

- NPOT texture:ES 2.0 僅支援 CLAMP + NEAREST(受 spec 限制);v1 支援
- Mipmap 與 NPOT:complete level required
- Cube map seam:v1 不做 seamless(硬體 seamless 貴)
- Texture fetch 跨 page:MMU fault routed back

## Verification Plan

1. Bit-exact vs sw_ref(所有 format、wrap、filter 組合)
2. Bilinear golden:known-answer test
3. Mipmap:LOD computation correctness
4. Cache miss pattern
5. ETC1 decode:golden block test

## Open Questions

- [ ] L1 size:8 KB 是否夠(hit-ratio sim) — Phase 2.x; current LT/CA bypass the cache.
- [ ] Trilinear:dedicated path 或 2× bilinear — Phase 2.x.
- [x] Aniso filter:**out of v1** (ES 2.0 非必要).
- [ ] ETC1 實作 inside L1 還是 decode 後才入 cache — Phase 2.x; only RGBA8 supported today.
