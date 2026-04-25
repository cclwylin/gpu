---
doc: Architecture Spec
version: 0.1 (draft)
status: in progress (Phase 0)
owner: E1
last_updated: 2026-04-25
---

# Architecture Spec

## 1. Overview

本文件描述 GPU 頂層架構、block 劃分、interface、dataflow。
ISA 另見 [`isa_spec.md`](isa_spec.md),MSAA 細節見 [`msaa_spec.md`](msaa_spec.md)。

## 2. Top-level Block Diagram

```
                     ┌──────────── APB CSR ───────────┐
                     ▼                                ▼
 ┌──────────────────────────────────────────────────────────┐
 │  CP ─► VF ─► SC ─► PA ─► TB ─► RS ─► SC(FS) ─► TMU ─┐   │
 │  │                                               │   │   │
 │  │                                       ┌───────▼─┐ │   │
 │  │                                       │   PFO   │ │   │
 │  │                                       └────┬────┘ │   │
 │  │                                            ▼      │   │
 │  │                            ┌──────────────────┐   │   │
 │  │                            │   TBF (64 KB)    │   │   │
 │  │                            └────────┬─────────┘   │   │
 │  │                                     ▼             │   │
 │  │                                 ┌───────┐         │   │
 │  │                                 │  RSV  │         │   │
 │  │                                 └───┬───┘         │   │
 │  │                                     ▼             │   │
 │  │                        ┌──────────────────┐       │   │
 │  └──────────── MMU ──► L2 ◄──── MC ──► AXI (fb/tex)  │   │
 │                         ▲                            │   │
 │                      PMU(perf counters + trace)      │   │
 └──────────────────────────────────────────────────────┘
```

實線 = data flow,block 對應頂層 abbreviation 見 Master Plan A9。

## 3. Dataflow Walkthrough

### 3.1 Command submission
1. Driver 寫 command ring buffer(DRAM),更新 CSR `CMD_TAIL`。
2. CP 從 `CMD_HEAD` 讀取 packet,解析 state update / draw。
3. Draw packet 觸發 VF。

### 3.2 Vertex stage
4. VF fetch vertex index + attribute(透過 MMU → L2 → DRAM),放入 SC input queue。
5. SC 以 warp (16 thread) 執行 VS,輸出 varying + clip-space position。
6. PA 做 perspective divide、viewport transform、clip、cull、primitive assembly。
7. PA 輸出 triangle 給 TB。

### 3.3 Tiling
8. TB 算 triangle 的 tile coverage bitmap,每個 tile 存一份 per-tile primitive list 到 DRAM。
9. 所有 draw 累積完成後(或 tile list 溢出),進 render phase。

### 3.4 Render phase(per tile)
10. RS 讀取 tile primitive list,對每個 triangle 做 edge function evaluation,
    產生每 pixel 的 **4-bit coverage mask**(4× MSAA)或 1-bit(1×)。
11. 覆蓋到的 pixel 批次成 fragment warp,送 SC 跑 FS(per-pixel,非 per-sample)。
12. FS 中若有 `texture()`,SC 送 request 給 TMU,TMU filter 後回填。
13. FS 輸出 color 給 PFO。
14. PFO 做 alpha-to-coverage(若開啟)→ per-sample depth/stencil test →
    per-sample blend → 寫入 TBF。
15. TBF 64 KB 在 tile 結束時被 RSV 讀出:
    - 若輸出 FBO 是 1×:RSV 對 4 sample 做 box-filter resolve,單樣本寫回 DRAM。
    - 若輸出 FBO 是 4× MSAA renderbuffer(僅限 implicit-resolve 的 GL_EXT 路徑):
      仍在 RSV 做 resolve 後寫回,v1 不支援 store multi-sample 回 DRAM。

## 4. Block Specifications

### 4.1 CP — Command Processor
- **Role**:parse ring buffer,maintain HW state,dispatch to VF / RS。
- **Interface**:AXI master(讀 ring)、APB slave(CSR)、內部 fanout 到各 state block。
- **Internal**:FSM + command decoder + state scoreboard。
- **詳見**:`docs/microarch/cp.md`(Phase 0 產出)

### 4.2 VF — Vertex Fetch
- **Role**:index fetch、attribute fetch、format conversion、post-transform vertex cache。
- **Interface**:接 CP draw command,出 vertex batch 給 SC。
- **Internal**:index decoder、attribute cache、format converter。

### 4.3 SC — Shader Core
- **Role**:unified VS/FS,SIMT 16-thread warp。
- **Interface**:TLM from VF/RS,TMU request,PFO fragment output。
- **Internal**:
  - Warp scheduler
  - 4 lane × vec4 ALU(per-lane predicate)
  - Register file:32× vec4 GPR × 16 thread = 2 KB per warp slot
  - Constant buffer(16× vec4)
  - L1 I$(每 warp slot 共享)
  - Special function unit(rcp/rsq/exp/log/sin/cos)
  - TMU interface、varying load/store unit
- **詳見**:`docs/microarch/sc.md`

### 4.4 PA — Primitive Assembly
- **Role**:perspective divide、viewport、clip、cull、assemble triangle。
- **Internal**:clip/cull FSM、viewport transform datapath、primitive buffer。

### 4.5 TB — Tile Binner
- **Role**:將 primitive 依 tile coverage 分類,產生 per-tile primitive list。
- **Internal**:bounding box compute、tile range iterator、DRAM writer。
- **Memory**:per-tile list 存在 DRAM,大小由 bin size register 控制。

### 4.6 RS — Rasterizer
- **Role**:edge function evaluation,產生 per-pixel coverage mask(MSAA-aware)。
- **Internal**:
  - Setup unit(compute edge equation)
  - Coarse raster(tile-level)
  - Fine raster(pixel-level,4 sample)
  - Coverage mask generator(4-bit per pixel)
- **Output**:fragment quad(2×2 pixel)+ per-pixel coverage mask。

### 4.7 TMU — Texture Unit
- **Role**:texel fetch、filter、format decode。
- **Internal**:
  - Address generator(2D / cube、mipmap selection)
  - L1 Tex$(direct-mapped,block size = 4×4 texel)
  - Format decoder(RGBA8 / RGB565 / ETC1)
  - Bilinear filter(trilinear 走 2 次 bilinear 合成)
- **Latency target**:miss path < 20 cycle avg。

### 4.8 PFO — Per-Fragment Ops
- **Role**:depth/stencil test、blend、alpha-to-coverage。
- **Internal**:
  - A2C unit(可選路徑,FS 後第一站)
  - Per-sample depth/stencil test(4× parallel)
  - Per-sample blend(4× parallel)
  - Coverage mask AND 邏輯
- **Output**:per-sample color/depth/stencil 更新到 TBF。

### 4.9 TBF — Tile Buffer
- **Role**:on-chip per-sample color/depth/stencil storage。
- **Size**:16 × 16 × 4 samples × (32 color + 24 depth + 8 stencil)bit = **64 KB**
- **Organization**:分 bank(預計 8 bank × 8 KB)避免 4× sample access conflict。
- **Access pattern**:PFO 每 cycle 寫入 4 sample × 1 pixel quad。

### 4.10 RSV — Resolve Unit
- **Role**:tile flush 時,讀 TBF 4 sample,box-filter 合成 1 pixel 寫出 DRAM。
- **Algorithm**:`out = (s0 + s1 + s2 + s3 + 2) >> 2`(per channel,round-to-nearest)。
- **Control**:依 FBO 配置決定 resolve 或直接 pass-through。

### 4.11 MMU
- **Role**:virtual → physical translation,支援 driver-managed page table。
- **Internal**:L1 TLB(fully-associative,16 entry)+ L2 TLB(4-way,256 entry)+ page walker。
- **Page size**:4 KB only。
- **Client**:VF / TMU / L2(fb)/ CP ring buffer。

### 4.12 L2 — Unified Cache
- **Role**:shared cache for texture + framebuffer + command ring。
- **Size budget**:256 KB(Phase 0 待確認)。
- **Organization**:8-way set associative,64 B line。

### 4.13 MC — Memory Controller
- **Role**:AXI4 master,request scheduling,QoS。
- **Interface**:2× 128-bit AXI master(`axi_tex`、`axi_fb`)。
- **QoS**:texture read 優先級 < framebuffer write(防止 stall)。

### 4.14 CSR — Register Block
- **Role**:APB slave,提供 register read/write interface。
- **Source of truth**:[`specs/registers.yaml`](../specs/registers.yaml),自動生成此 block。

### 4.15 PMU — Performance Monitor Unit
- **Role**:perf counter + trace buffer。
- **Counter**:64× 32-bit,可選事件源(任一 block 都可 export event)。
- **Trace**:circular buffer in on-chip SRAM,可 dump 出 off-chip 或 DRAM。

## 5. Clock / Reset / Power

### 5.1 Clock
- Single domain `clk`。
- Target:1 GHz @ 28nm,500 MHz @ FPGA。
- RTL 內**禁止** gated clock(clock-gate cell 合成後由 EDA 插)。

### 5.2 Reset
- Async assert、sync de-assert 的 `rst_n`。
- Reset tree 由 top-level 統一,各 block 有 local synchronizer。

### 5.3 Power(留鉤)
- Single power rail for v1。
- 每 module 有 `clk_en` port,提供未來 clock gating 切入。
- Power domain partition 留到 tape-out 前評估。

## 6. Memory Map(頂層)

| Region | Address Range | 用途 |
|---|---|---|
| CSR | `0x0000_0000 – 0x0000_FFFF` | APB register space |
| Perf counter | `0x0001_0000 – 0x0001_FFFF` | PMU |
| (reserved) | `0x0002_0000 – 0x0FFF_FFFF` | 未定 |
| DRAM aperture | `0x1000_0000+` | Driver-managed |

完整 register 配置見 [`specs/registers.yaml`](../specs/registers.yaml)。

## 7. Interface Summary(Phase 0 凍結)

| Port | Direction | Width | Protocol |
|---|---|---|---|
| `clk` | in | 1 | — |
| `rst_n` | in | 1 | async assert / sync deassert |
| `axi_tex_*` | master | 128 | AXI4 |
| `axi_fb_*` | master | 128 | AXI4 |
| `apb_*` | slave | 32 | APB |
| `irq` | out | 1 | level |
| `trace_*` | out | N | proprietary |

詳細 port list 由 `specs/registers.yaml` + interface spec 展開(Phase 0 產出)。

## 8. Open Questions(Phase 0 要解)

- [ ] L2 size 256 KB vs 512 KB(area vs miss rate 模擬後定)
- [ ] Warp 大小 16 是否再調(資料 → Phase 1 trace 佐證)
- [ ] TLB entry 數足夠度驗證
- [ ] Hier-Z 是否納入 v1(面積 vs 效能)
- [ ] Shader core 數量(1 core vs N core,依 target FPS)

## 9. References

- [`MASTER_PLAN.md`](MASTER_PLAN.md)
- [`isa_spec.md`](isa_spec.md)
- [`msaa_spec.md`](msaa_spec.md)
- OpenGL ES 2.0 Spec(Khronos)
- PowerVR TBDR whitepapers(公開資料)
