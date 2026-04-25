# VF — Vertex Fetch

**Role**:Index fetch、attribute fetch、format conversion、post-transform vertex cache。

## Interface
- In:from CP(draw packet)
- Out:vertex batch → SC(VS input queue)
- Memory:MMU client(index + attribute buffer read)

## Internal
- Index decoder(u8 / u16 / u32)
- Attribute cache(LRU,hit ratio target > 70%)
- Format converter(int / fixed / float / normalized)
- Post-transform vertex cache(避免重跑 VS,size TBD Phase 0)

## Owner
E1
