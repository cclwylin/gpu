# PFO — Per-Fragment Ops

**Role**:Alpha-to-coverage、per-sample depth/stencil test、per-sample blend。

## Interface
- In:from SC(FS output:color + coverage mask)
- TBF:read-modify-write(per-sample color / depth / stencil)
- PMU:event(a2c hit、depth fail count 等)

## Pipeline order
```
FS color + mask
     │
     ▼
  A2C?  ──►  mask' = mask & a2c_mask(alpha)
     │
     ▼
  Per-sample depth test  (4-lane parallel)
     │
     ▼
  Per-sample stencil test
     │
     ▼
  Per-sample blend       (4-lane parallel)
     │
     ▼
   TBF write (per sample)
```

## MSAA
- Path 固定 4× wide(bank-aware)
- Blend read 4 sample × 32b color,write back 4 sample
- Bank conflict 處理(若相鄰 quad 撞 bank)

## Owner
E2
