# ref_shader_2 — Phong Lighting(per-fragment)

ISA 表達力驗證 shader #2。經典的 normal 傳遞 + per-fragment Phong,
壓力測試 transcendental(rsq)、3-operand mad、normalize、reflect。

## GLSL source
- [`vertex.glsl`](vertex.glsl)
- [`fragment.glsl`](fragment.glsl)

## Hand-written assembly
- [`vertex.asm`](vertex.asm)
- [`fragment.asm`](fragment.asm)

## ISA coverage
| Feature | Used? |
|---|---|
| `mad`(3-operand) | ✓ |
| `dp3` | ✓(normal dot product) |
| `rsq` | ✓(normalize) |
| `max` / `min` | ✓(clamp) |
| `mul` 密集 | ✓ |
| Swizzle `.xyz` | ✓ |
| Varying pack | ✓(normal + world-pos 擠 2 varying) |

## Verification plan
- Normalize 會 exercise `rsq` accuracy(ISA spec 容忍 3 ULP)
- Expected:tolerance-based pixel diff(non bit-exact due rsq)
