# RSV — Resolve Unit

**Role**:Tile flush 時,4 sample → 1 pixel box-filter resolve,寫出 DRAM。

## Interface
- In:TBF(read 4 sample × 32b color per pixel)
- Out:DRAM write(post-resolve RGBA8),via MC
- Control:CSR `FBO_CTRL.RESOLVE_MODE`

## Algorithm(box filter)
```
for each pixel (x, y) in tile:
  for each channel c in {R,G,B,A}:
    sum = s0[c] + s1[c] + s2[c] + s3[c]
    out[c] = (sum + 2) >> 2       // integer round-to-nearest
  write out -> DRAM[fb_base + offset(x,y)]
```

## Throughput
- Target 1 pixel / cycle
- 4-sample wide datapath

## Usage
- Implicit resolve on tile flush(primary path)
- Explicit resolve for `glBlitFramebuffer`(MSAA → 1×)走同一硬體

## Owner
E2
