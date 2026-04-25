# TBF — Tile Buffer

**Role**:On-chip per-sample color / depth / stencil storage。

## Size
- 16 × 16 pixel × 4 sample × (32b color + 24b depth + 8b stencil)
- ~64 KB(Phase 2 定案 exact layout,含 double buffer 或 metadata)

## Interface
- Write:from PFO(per sample)
- Read:from RSV(tile flush)
- Read / Write:from PFO for blend
- BIST port(via CSR)

## Bank
- 8 bank × 8 KB(初始設計,Phase 2 改)
- Policy:相鄰 pixel 分散到不同 bank

## Owner
E2
