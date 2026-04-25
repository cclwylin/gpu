# PA — Primitive Assembly

**Role**:Perspective divide、viewport transform、clipping、culling、assemble triangle。

## Interface
- In:vertex from SC(post-VS)
- Out:triangle(NDC + screen space)→ TB

## Internal
- Perspective divide(vec4 → vec3)
- Viewport transform
- Clipping(near/far,v1 不做 user clip plane)
- Back-face cull
- Triangle assembly(strip / fan / list 展開)

## Output format
- 3 vertex × (screen xy + depth + varying)
- 含 primitive ID / state pointer

## Owner
E1
