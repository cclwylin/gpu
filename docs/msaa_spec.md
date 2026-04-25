---
doc: MSAA 4x Spec
version: 0.1 (draft)
status: in progress (Phase 0)
owner: E2
last_updated: 2026-04-25
---

# MSAA 4× Spec

4× MSAA(Multi-Sample Anti-Aliasing)實作規格。
Per-pixel shading + per-sample coverage/depth/stencil。

## 1. Overview

```
                     Rasterizer
                         │
                         ▼
           ┌─────────────────────────┐
           │  Coverage Mask (4-bit)  │  ← per pixel
           └─────────────┬───────────┘
                         │
                   (per-pixel shading)
                         │
                         ▼
                 Fragment Shader
                         │ color (1 value per pixel)
                         ▼
                Alpha-to-Coverage?  ─► modify mask
                         │
                         ▼
          ┌──────────────────────────┐
          │  Per-Sample Depth Test   │  ← 4 lanes parallel
          │  Per-Sample Blend        │
          └──────────────┬───────────┘
                         │
                         ▼
            Tile Buffer (64 KB, per sample)
                         │
                 (tile flush)
                         ▼
                    Resolve Unit
                  (4 sample → 1 pixel)
                         │
                         ▼
                   DRAM framebuffer
```

## 2. Sample Pattern

D3D 10.1 rotated-grid 4×,hard-coded。
Sample offsets 以 pixel 中心為原點,單位 1/16 pixel。

| Sample | dx | dy |
|---|---|---|
| 0 | -2/16 | -6/16 |
| 1 |  6/16 | -2/16 |
| 2 | -6/16 |  2/16 |
| 3 |  2/16 |  6/16 |

```
 +──────+──────+──────+──────+
 │      │      │   1  │      │
 +──────+──────+──────+──────+
 │   0  │      │      │      │
 +──────+──── center ──+──────+
 │      │      │      │      │
 +──────+──────+──────+──3───+
 │      │   2  │      │      │
 +──────+──────+──────+──────+
```

Sample location **固定**,不做 `GL_NV_sample_locations` 類型的 programmable 功能。

## 3. Coverage Mask

- 4-bit per pixel,bit `i` 代表 sample `i` 是否被 triangle 覆蓋。
- Rasterizer 對每個 sample 點求 edge function,同時產 4 個 sample 的 inside 結果。
- Initial state:由 rasterizer 計算。
- 後續被修改的時機:
  1. Alpha-to-coverage(§5)
  2. `kil`/`discard`(整個 pixel mask 清零)
  3. Per-sample depth/stencil test(fail 的 sample bit 清零)

## 4. Shading Rate

**Per-pixel shading**:
- 整個 pixel 若 `mask != 0` 則 FS 跑一次。
- FS 的 `gl_FragCoord` 採 pixel 中心(0.5, 0.5)。
- Derivatives(dFdx/dFdy)以 2×2 pixel quad 計算,不以 sample。
- FS 輸出的 color 會**複製**寫入被 mask 覆蓋的每個 sample。

**不做**:
- Sample shading(per-sample FS 執行)
- `gl_SampleID`、`gl_SampleMask`、`gl_SamplePosition`(ES 3.1 feature)

效能優勢:4× MSAA 時 shader cost ≈ 1×(僅 coverage/depth/blend 付 4× 代價)。

## 5. Alpha-to-Coverage

啟用條件:`glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE)`。

### 5.1 演算法
FS 輸出 alpha 後:
1. 將 alpha ∈ [0, 1] 映射到 4-bit coverage mask。
2. Mask AND 原本的 coverage mask。

### 5.2 Mapping 規則

|  alpha range   | new mask bits set |
|---|---|
| [0.000, 0.125) | 0000 |
| [0.125, 0.375) | 0001 |
| [0.375, 0.625) | 0101 |
| [0.625, 0.875) | 0111 |
| [0.875, 1.000] | 1111 |

選擇對稱 pattern(0101 vs 1010 二選一),避免視覺偏斜。確切 pattern Phase 0 定案。

### 5.3 Alpha Value
- 取 color output 第一顆 `o0.w`。
- 在 a2c path 後,alpha **不會**自動被改成 1.0(OpenGL 4.0+ 的 `GL_SAMPLE_ALPHA_TO_ONE` 不做)。

## 6. Per-Sample Depth/Stencil

- 每個 sample 獨立 depth test + stencil test。
- Depth value 在每個 sample 位置獨立 interpolation(barycentric × sample offset)。
- Early-Z:在 SC 前做,但必須 **per-sample**:
  - 若該 pixel 所有 sample 都 fail early-Z → skip FS。
  - 若部分 fail → 只更新 mask,仍跑 FS。
- Late-Z(FS 有 `kil`/`discard` 或修改 depth):fallback 到 PFO stage。

## 7. Blend

- 每個 sample 獨立 blend,讀 TBF 該 sample 的舊 color,與 FS 輸出 color 做 blend。
- Blend equation 全部走 ES 2.0 既定(無 dual-source、無 logic op)。
- ROP datapath 4× wide:4 sample 並行 blend。

### 7.1 Bank 配置
TBF 64 KB 分 8 bank × 8 KB:
- 每 bank 存 2×2 pixel × 4 sample × (color+depth+stencil)
- 相鄰 pixel 分散到不同 bank,避免 quad access 衝突
- Bank arb:round-robin,worst-case 1 cycle/quad

## 8. Tile Buffer Layout

```
Tile = 16 × 16 pixel × 4 sample
Per sample = 32b color + 24b depth + 8b stencil = 64 bit = 8 byte
Per pixel  = 4 × 8 B = 32 byte
Per tile   = 256 pixel × 32 B = 8192 B (color+depth+stencil 合計)

實際 Phase 0 確認:64 KB 是否含 double buffer,或單 tile 8 KB + metadata?
```

**Note**:Master Plan 引用 64 KB 為上限(含 3 channel × 4 sample 全展開 +
margin)。Phase 2 定案最終配置後更新。

## 9. Resolve Unit(RSV)

### 9.1 Trigger
Tile flush 時 RSV 讀 TBF,做 resolve 並寫 DRAM。

### 9.2 Algorithm(box filter)
```
for each pixel (x, y) in tile:
    for each channel c in {R, G, B, A}:
        sum = s0[c] + s1[c] + s2[c] + s3[c]
        out[c] = (sum + 2) >> 2     // round-to-nearest
    write out to DRAM[fb_addr + offset(x,y)]
```

### 9.3 Format
- Input:4 × RGBA8(per sample in TBF)
- Output:1 × RGBA8(to DRAM)
- 不支援 float resolve(v1 沒有 float color format)

### 9.4 Throughput
- 目標 1 pixel/cycle resolve。
- 4-sample wide datapath,no bubbles on simple case。

### 9.5 `glBlitFramebuffer` 路徑
- App-triggered resolve(MSAA FBO → 1× FBO)走同一 RSV hardware。
- Driver 產生 resolve-only command,不經過 render pipeline。

## 10. Interaction with `discard` / `kil`

- `discard` 在 FS 內呼叫 → 該 pixel 整個 mask = 0。
- 與 a2c 並存時:`discard` 優先,a2c 的 mask 被覆蓋為 0。
- 與 per-sample depth test 並存:先 discard(mask 清零)→ depth test 全 fail → 無寫入。

## 11. API Surface(driver 要支援)

- `glRenderbufferStorageMultisample(target, 4, internalfmt, w, h)`
  - `samples` 必須 = 4(其他值報 `GL_INVALID_OPERATION`)
- `glFramebufferRenderbuffer(... with MSAA RB)`
- `glBlitFramebuffer(srcX0, ..., dstX1, GL_COLOR_BUFFER_BIT, GL_NEAREST)`
  - 僅支援 MSAA → 1× 方向
  - 僅支援 1:1 size(不做 scale)
- `glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE)`
- **不支援**:`glGetTexImage` from MSAA texture(因為不支援 MSAA texture sample)

## 12. Register State

MSAA-related CSR(完整定義見 [`specs/registers.yaml`](../specs/registers.yaml)):

| Register | 說明 |
|---|---|
| `FBO_CFG.msaa_enable` | 1 = 4× MSAA,0 = 1× |
| `FBO_CFG.a2c_enable` | Alpha-to-coverage 啟用 |
| `FBO_CFG.resolve_mode` | `implicit` / `explicit` |
| `TBF_CFG.bank_config` | Tile buffer bank 配置 |
| `PERF.msaa_*` | MSAA-specific counters |

## 13. Perf Counters(MSAA-specific)

| Counter | 說明 |
|---|---|
| `coverage_hist_0..4` | 每個 mask population count(0/1/2/3/4 bit set) |
| `resolve_cycle` | Resolve unit 活動 cycle 數 |
| `tbf_spill` | Tile buffer spill to DRAM 次數(不應發生,若 >0 為 bug) |
| `a2c_hit` | a2c 改變 mask 的次數 |
| `early_z_per_sample_kill` | per-sample early-Z kill 的 sample 數 |

## 14. Open Questions(Phase 0 要解)

- [ ] Alpha-to-coverage mapping pattern 0101 vs 1010 對稱選擇
- [ ] Tile buffer bank 數:8 bank 是否足夠(worst case 4 sample 同時 read-modify-write)
- [ ] Hier-Z / per-tile min-max depth 是否 v1 納入
- [ ] Early-Z per-sample 硬體結構(4× compare unit 還是 1×4 cycle)
- [ ] Resolve unit 放 TBF 內還是獨立 block(面積 vs timing)

## 15. Validation Plan

Phase 1 exit 前要驗證:
1. **Rotated-grid coverage**:斜線 triangle 的 coverage mask 正確性
2. **A2C mapping**:alpha 掃描 0→1,coverage bits 單調遞增
3. **Resolve correctness**:bit-exact box filter(整數算術 no rounding diff)
4. **MSAA vs 1× diff**:邊緣 SSIM > 0.99,非邊緣 pixel 完全相同
5. **Perf model**:4× MSAA FPS loss 在 15–25% 區間(複雜 scene)

## 16. References

- [`MASTER_PLAN.md`](MASTER_PLAN.md)
- [`arch_spec.md`](arch_spec.md)
- OpenGL ES 2.0 spec + `GL_EXT_multisampled_render_to_texture`
- D3D 10.1 Multisample Patterns(Microsoft)
