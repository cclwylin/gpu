# ref_shader_3 — Branch + Loop(Control Flow Stress)

ISA 表達力驗證 shader #3。壓力測試 divergent CF 與 predication。

## GLSL source
- [`vertex.glsl`](vertex.glsl)(極簡,僅 pass)
- [`fragment.glsl`](fragment.glsl)(核心)

## Hand-written assembly
- [`fragment.asm`](fragment.asm)(VS trivial,只列 FS)

## 設計
Circle-based discard + dynamic-loop procedural color:
- 離中心點 > 某半徑 → `discard`
- 在半徑內,依 uv 跑 N 次 iterative refinement(仿 Mandelbrot 初階)
- 迭代命中條件時 `break` 提前離開

## ISA coverage
| Feature | Used? |
|---|---|
| Predicate | ✓ |
| `if_lt` / `if_gt`(mapped to cmp + bra) | ✓ |
| `loop` / `endloop` | ✓ |
| `break` | ✓ |
| `kil` | ✓(discard) |
| `mad` in loop body | ✓ |
| `cmp` 3-operand | ✓ |

## Verification
- 由於 iterative 依賴 `mad` 精度,最終 color 以 ULP tolerance 比對
- Discard 區域 pixel 不寫入 → 直接檢查 mask
