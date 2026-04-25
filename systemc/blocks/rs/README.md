# RS — Rasterizer(MSAA-aware)

**Role**:Edge function evaluation,產生 per-pixel coverage mask(4-bit for 4× MSAA)。

## Interface
- In:triangle from TB(per-tile primitive list,由 tile driver 給)
- Out:fragment quad(2×2 pixel)+ per-pixel coverage mask → SC(FS)

## Internal
- Setup unit(compute edge equation)
- Coarse raster(tile-level reject)
- Fine raster(pixel-level)
- **Coverage mask generator**(per pixel 對 4 sample 算 inside)
- Barycentric coordinate computation(varying interp 用)

## MSAA 特別
- 4-sample evaluation per pixel(rotated-grid pattern,見 [msaa_spec.md §2](../../../docs/msaa_spec.md))
- 1× mode:compatible path,mask 簡化為 1-bit

## Output
- 2×2 quad + 4 × 4-bit mask(total 16 bit per quad)

## Owner
E2
