# ref_shader_1 — Gouraud VS + Textured FS

ISA 表達力驗證 shader #1,涵蓋:
- Vec4 ALU + swizzle + write mask
- Matrix 轉換(dp4)
- Varying output/input
- Texture 2D sample
- Mov + mul

## GLSL source
- [`vertex.glsl`](vertex.glsl)
- [`fragment.glsl`](fragment.glsl)

## Hand-written assembly
- [`vertex.asm`](vertex.asm)
- [`fragment.asm`](fragment.asm)

## Expected behavior
- VS:`gl_Position = u_mvp * a_pos`,`v_color = a_color * u_tint`,`v_uv = a_uv.xy`
- FS:`gl_FragColor = texture2D(u_tex, v_uv) * v_color`

## ISA coverage
| Feature | Used? |
|---|---|
| `mov`, `mul`, `mad` | ✓ |
| `dp4` | ✓ (MVP 轉換) |
| Swizzle | ✓ |
| Write mask | ✓ |
| Varying | ✓ |
| Texture `tex` | ✓ |
| Predication | — |
| Flow control | — |
| SFU (rcp/rsq/...) | — |

## Verification plan
- ISA simulator 執行 → 比對 sw_ref
- Bit-exact(UNORM8 格式)
