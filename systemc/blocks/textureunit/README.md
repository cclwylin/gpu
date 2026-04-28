# TMU — Texture Unit

**Role**:Texture sampling(fetch、filter、decode)。

## Interface
- In:SC(FS)texture request(binding slot + uv + mode)
- Out:filtered texel → SC
- Memory:MMU client(texel read via L1 Tex$ → L2)

## Internal
- Address generator(2D / cube、wrap mode、mipmap selection)
- **L1 Tex$**(direct-mapped,block = 4×4 texel,LRU)
- Format decoder(RGBA8 / RGB565 / ETC1)
- Bilinear filter(8-lerp path)
- Trilinear:2× bilinear + lerp(分兩 cycle)

## Modes
- plain tex
- `texb`:bias(加到 LOD)
- `texl`:explicit LOD
- `texg`:explicit gradient(dFdx / dFdy)

## Target
- Avg latency < 20 cycle(miss path)
- L1 Tex$ hit ratio > 80% on typical workload

## Owner
E2
