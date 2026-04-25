---
block: PA
name: Primitive Assembly
version: 0.1 (draft)
owner: E1
last_updated: 2026-04-25
---

# PA — Primitive Assembly Microarchitecture

## Purpose

接 VS 輸出的 clip-space vertex,做:
1. Perspective divide(clip → NDC)
2. Viewport transform(NDC → screen-space)
3. Clipping(near / far plane)
4. Back-face culling
5. Triangle assembly(strip / fan / list 展開為獨立 triangle)
6. Output triangle 給 TB

## Block Diagram

```
  SC (VS out: clip xyzw + varying)
          │
          ▼
   ┌──────────────┐
   │ Vertex Queue │
   └──────┬───────┘
          ▼
   ┌──────────────────┐
   │ Primitive Assembler  (3 vertex gather)
   └──────┬───────────┘
          ▼
   ┌──────────────────┐
   │ Clip Test (near/far)
   └──┬───────────┬───┘
      │pass       │clip needed
      ▼           ▼
      │       ┌────────┐
      │       │ Clipper │ (Sutherland–Hodgman subset)
      │       └────┬───┘
      └────────────┤
                   ▼
            Persp Divide
                   │
                   ▼
             Viewport xform
                   │
                   ▼
              Backface cull
                   │
                   ▼
                  TB
```

## Interface

| Port | Dir | Notes |
|---|---|---|
| `sc_vs_out_*` | in | vertex + varying(clip-space) |
| `tb_tri_*` | out | triangle(screen-space) |
| `csr_*` | in | viewport、cull config、primitive mode |

## Clipping

- 只做 **near / far plane** clip(v1 不做 user clip plane)
- 其他 plane(L/R/T/B)靠 viewport + scissor,不在 vertex level clip
- Sutherland-Hodgman:1 triangle 最多變 2 triangle(near/far 各切一次)
- 切出的新 vertex 做 varying interpolation

## Perspective Divide

- `x /= w; y /= w; z /= w; 1/w = rcp(w)`
- 用 SC 的 rcp（或 PA 自己的 rcp 小 SFU,Phase 0 定)
- NaN / inf 處理:degenerate triangle cull

## Viewport Transform

- `screen_x = (NDC.x + 1) * 0.5 * width + vp.x`
- `screen_y = (NDC.y + 1) * 0.5 * height + vp.y`(y-flip depend on convention)
- `screen_z = (NDC.z + 1) * 0.5 * (far - near) + near`
- 以 16.8 fixed-point 輸出(sub-pixel precision 1/256)

## Backface Cull

- 2D signed area:`(x1-x0)*(y2-y0) - (x2-x0)*(y1-y0)`
- `> 0` → CCW;`< 0` → CW
- Per-state winding + `GL_CULL_FACE` 決定 cull / keep

## Primitive Modes(ES 2.0)

| Mode | Input | Output triangle 數 |
|---|---|---|
| TRIANGLES | 3 vertex | 1 |
| TRIANGLE_STRIP | 1 vertex per new tri | 1 after initial 3 |
| TRIANGLE_FAN | 1 vertex per new tri | 1 after initial 3 |
| POINTS | 1 vertex | 特殊(v1 轉成 2-tri quad by TB 或 RS) |
| LINES / LINE_STRIP / LINE_LOOP | 2 vertex | 特殊(expand to thin quad) |

Points / lines 的 expansion 位置(PA 或 RS)Phase 0 決定。建議 **PA**。

## Throughput

- Target:1 triangle / 2 cycle(amortized)
- Limiter:SFU(rcp)若 shared 則會 stall

## Corner Cases

- Degenerate triangle(2 vertex 重合或 collinear):cull
- Zero-area viewport:no triangle emitted
- All-clipped triangle:cull
- Triangle fully behind near:cull

## Verification Plan

1. 每種 primitive mode 獨立 test
2. Clip test:triangle 跨 near、fully behind、fully in
3. Degenerate case
4. Floating-point corner:very small w、inf、nan
5. Per-pixel check bit-exact vs sw_ref

## Open Questions

- [ ] Points / lines expansion 位置(PA vs RS)
- [ ] Sub-pixel precision:16.8 vs 16.4(Phase 0 驗證圖質影響)
- [ ] PA 獨立 rcp 還是走 SC SFU(latency vs 面積)
