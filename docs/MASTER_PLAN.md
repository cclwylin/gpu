---
doc: MASTER_PLAN
version: 1.0
status: frozen (Phase 0 kickoff)
owners: E1, E2, E3
last_updated: 2026-04-25
---

# GPU Master Plan v1.0

OpenGL ES 2.0 Programmable GPU,TBDR + Unified SIMT Shader + 4× MSAA,
目標 synthesizable RTL tape-out-ready @ 28nm。

本文為 **single source of truth**,變更必須走 PR review。

---

## Part A — Frozen Architecture Specs

### A1. Pipeline

| 項目 | 規格 |
|---|---|
| Rendering | TBDR,tile = 16 × 16 pixel |
| Shader | Unified(VS/FS 共用),SIMT |
| Warp | 16 threads(4 lane × 4 thread) |
| 頻率 | 1 GHz @ 28nm / 500 MHz FPGA |
| Peak throughput | 1 pixel/cycle(1×),~0.8 pixel/cycle(4× MSAA) |
| Vertex peak | 2 vertex/cycle |

### A2. OpenGL ES 2.0 Feature Set

**做**:
- Full ES 2.0 core + `GL_EXT_multisampled_render_to_texture` 式 4× MSAA
- Alpha-to-coverage
- Texture:RGBA8、RGB565、ETC1
- `glBlitFramebuffer`(MSAA → 1× resolve)

**不做**(明確排除):
- Sample shading、MSAA texture sample(把 MSAA FBO 當 texture 讀)
- Geometry / tessellation / compute / transform feedback
- ES 3.0 以上 feature

### A3. MSAA 4× 規格

- Sample pattern:D3D rotated-grid 4×,hard-coded
- Per-pixel shading + per-sample coverage/depth/stencil
- Coverage mask:4-bit per pixel
- Alpha-to-coverage:在 PFO 前修改 coverage mask
- Resolve:box filter(4 sample 平均),hardware fixed-function,tile-flush 時執行
- Resolve format:RGBA8 only
- Tile buffer:64 KB/tile(16×16 × 4 samples × (color32 + depth24 + stencil8))

### A4. Shader ISA(v0.1 draft,Phase 0 結束前 freeze 到 v1.0)

```
32-bit fixed-width instruction
32× vec4 GPR per thread, 16× vec4 constant, per-lane predicate register
Vec4 ALU with per-component write mask and source swizzle

ALU  : add mul mad dp3 dp4 rcp rsq exp log sin cos min max abs frc flr cmp mov
Flow : bra call ret loop endloop break  (structured only)
Mem  : ld st   (uniform / varying / output)
Tex  : tex texb texl texg   (2D / cube, bias / lod / grad)
Misc : kil nop

Divergence: per-lane execution mask + predication
FP format: IEEE754 binary32,denormal flush-to-zero,round-to-nearest-even
```

詳見 [`docs/isa_spec.md`](isa_spec.md)。

### A5. Memory / Bus

- **對外 bus**:AXI4 master × 2(`axi_tex` 128-bit、`axi_fb` 128-bit),APB slave(CSR)
- **MMU**:2-level TLB,4 KB page,hardware page walker
- **Caches**:L1 I$、L1 Tex$、L2 unified
- **Tile buffer**:on-chip SRAM 64 KB/tile
- **DRAM model**:DRAMSim3

### A6. Command / Register

- Command stream:ring buffer,CP pull
- CSR:APB,memory-mapped
- Perf counter:64× 32-bit,含 MSAA-specific(coverage histogram、resolve cycle、tile spill)
- Register map single source of truth:[`specs/registers.yaml`](../specs/registers.yaml),
  自動生 C header / SystemC / RTL / driver / doc

### A7. Clock / Reset / Power

- Single clock domain
- Async reset,synchronous de-assertion
- 每 module top 留 `clk_en` port(clock-gate cell 後期插)
- Single power rail(ASIC 後期再評估 power gating)

### A8. DFT / Debug

- Scan-ready RTL coding style day 1(no latch、no combinational loop、no gated clock in RTL)
- Trace port:pipeline event dump 到 off-chip(FPGA debug)
- SRAM BIST:涵蓋 tile buffer、L1/L2、TLB

### A9. 頂層 Block 清單

| Abbr | Block | 職責 |
|---|---|---|
| CP  | Command Processor | Ring buffer parse,state update |
| VF  | Vertex Fetch | Index/attribute fetch + cache |
| SC  | Shader Core | Unified SIMT shader |
| PA  | Primitive Assembly | Clip / cull / viewport |
| TB  | Tile Binner | Primitive → tile bucket |
| RS  | Rasterizer | Edge function + coverage mask |
| TMU | Texture Unit | Filter + L1 Tex$ |
| PFO | Per-Fragment Ops | Depth / stencil / blend / a2c |
| TBF | Tile Buffer | 64 KB on-chip,per-sample |
| RSV | Resolve Unit | 4→1 box filter |
| MMU | Memory Management Unit | TLB + page walker |
| L2  | L2 Cache | Unified |
| MC  | Memory Controller | AXI4 master |
| CSR | Register Block | APB slave |
| PMU | Perf/Monitor Unit | Perf counters + trace |

### A10. Verification Methodology

| Level | Framework |
|---|---|
| Block | UVM-SystemC |
| System | cocotb + Python(scene regression) |
| Formal | SymbiYosys(CP FSM、arbiter、MMU、resolve) |
| Coverage 目標 | line 100% / toggle 98% / functional 100% |

---

## Part B — Shader Compiler Stack

```
GLSL (ES 2.0)
   │  glslang (third_party)
   ▼
SPIR-V
   │  自家 SPIR-V reader
   ▼
Custom SSA IR
   │  opt passes: const-fold / DCE / CSE / coalesce /
   │              swizzle opt / co-issue pair / schedule
   ▼
Scheduled IR
   │  register allocator (linear scan, spill to scratch)
   │  ISA encoder
   ▼
Shader binary + metadata (varying/uniform layout, tex bindings)
```

**設計原則**:
- SPIR-V 當 IR entry point,為 Vulkan/其他 frontend 預留
- SSA + typed IR,register allocator 可獨立演進
- GPU-aware optimizer:swizzle、co-issue、predicate
- Day-1 correctness,optimizer 分階段加
- ABI 在 Phase 0 凍結:varying layout、uniform layout、predicate reg 編號、call convention

配套工具:assembler、disassembler、ISA simulator(驗證 compiler output)。

---

## Part C — 28-Month Phase Plan

### Phase 0 — Architecture Freeze(M0 – M2.5,2.5 mo,全員)

Deliverables:
- Arch spec(~150 頁),ISA spec,MSAA spec,register map YAML,interface spec
- Toolchain freeze:SystemC 2.3.3、UVM-SystemC、Verilator、Icarus、cocotb、DRAMSim3、glslang、SymbiYosys
- Repo skeleton、CI、coding style、review process
- ISA 表達力驗證:3 個 reference shader 手寫 assembly 跑通 ISA sim

**Exit criteria**:Spec review signed off,no open arch question。

### Phase 1 — Three-Track Parallel(M2.5 – M9,6.5 mo)

**Track SW Ref Model** — E3
- ES 2.0 state machine,所有 pipeline stage,per-sample raster / depth / stencil
- Resolve、alpha-to-coverage
- FP 實作與 HW 目標 bit-accurate
- Stage boundary trace dump 格式定稿
- **Exit**:ES 2.0 CTS subset + MSAA tests 全綠

**Track Compiler** — E3(共享)
- glslang 整合、SPIR-V reader、SSA IR、basic codegen
- Assembler / disassembler / ISA simulator
- **Exit**:~500 shader corpus 全 compile,ISA sim 結果 = ref model

**Track HW TLM-LT** — E1 + E2
- 所有 block functional-only SystemC,TLM-2.0 對外
- Raster 含 coverage 輸出、ROP 4× wide datapath、Resolve Unit
- Shader core 包裝 Compiler 的 ISA simulator
- **Exit**:full-chip TLM 跑 reference scene(1× + 4× MSAA)framebuffer bit-exact vs ref model

### Phase 2 — Cycle-Accurate SystemC(M9 – M16,7 mo)

- 所有 block → cycle-accurate(`sc_signal` + `SC_CTHREAD` + clk + ready/valid),pin-level,synthesizable subset coding
- FIFO / arbiter / back-pressure / pipeline stage 實體化
- Tile buffer 64 KB 分 bank,per-sample access pattern
- ROP 4× wide pipeline 設計(bank conflict 處理)
- Resolve Unit cycle-accurate
- MMU + TLB + caches 實體化,L2 接 DRAMSim3
- Per-sample early-Z
- Performance model:量化 1× vs 4× MSAA delta
- Co-simulation:每 stage output vs TLM(latency 可容,內容需一致)

**Exit**:3 reference scene @ target FPS、coverage 達門檻、1×/4× FPS delta 在預估區間。

### Phase 3 — RTL Conversion(M16 – M23.5,7.5 mo)

- 手寫 Verilog,SystemC cycle-accurate model 當 golden reference
- Block-by-block UVM equivalence check
- Formal verification:CP FSM / arbiter / MMU / resolve control
- ROP 4× wide timing closure(預期 critical path)
- Tile buffer SRAM compile、bank 分割、BIST 插入
- Synthesis @ 1 GHz 28nm / 500 MHz FPGA
- Lint / CDC / STA 全綠

**Exit**:RTL vs CA equivalence 全綠,synth 收斂,signoff lint 通過。

### Phase 4 — Integration / FPGA / Tape-out Prep(M23.5 – M28,4.5 mo)

- FPGA emulation(VCU118 等級)
- Linux driver(EGL + GLES 2.0),Mesa-style 接口
- Real app:glmark2 subset、dEQP MSAA subset
- DFT insertion、pad ring、package
- Signoff:STA / power / DRC / LVS / ATPG coverage

**Exit**:Tape-out ready。

---

## Part D — Team 分工(2–3 FTE)

| | Phase 0 | Phase 1 | Phase 2 | Phase 3 | Phase 4 |
|---|---|---|---|---|---|
| **E1** Arch + HW lead | Spec 主筆 | TLM: CP / SC / PA / MMU | CA: SC + MMU | RTL: SC + MMU | Integration |
| **E2** HW + Verif | Spec: RS / PFO / RSV | TLM: RS / TMU / PFO / TBF / RSV | CA: RS / TMU / PFO / RSV / TBF | RTL: fixed-function + tb | FPGA bring-up |
| **E3** SW + Compiler | Spec: ISA / ABI / driver | Ref model + compiler | Compiler opt + driver skeleton | Driver + conformance | App enablement |

---

## Part E — Infrastructure(Phase 0 建好)

詳見 [top-level README](../README.md) 的目錄結構。

必備 infra:
- CI:lint + build matrix + smoke(每 commit)+ nightly full regression
- Coverage dashboard、perf dashboard(FPS / cycle / BW 趨勢)
- Golden trace DB(Git-LFS 或 S3)
- Bug tracker 分類:`arch / sw-ref / compiler / tlm / ca / rtl / tb / driver`
- Doc-as-code,spec 變更走 PR review

---

## Part F — Risk Register

| Risk | Impact | Mitigation |
|---|---|---|
| ISA 表達力不足 | 推翻 shader core + compiler | Phase 0 手寫 3 shader 驗證 |
| FP 精度不一致 | Ref/HW mismatch | Ref model 與 HW 共用 FP 實作 |
| Tile buffer BW 撞牆(4× MSAA) | 效能達不到 | Phase 2 早期跑 BW model,調 bank/tile size |
| ROP 4× wide timing 不收斂 | Critical path | Phase 2 跑 synth trial,必要時退 2 cycle/pixel |
| Compiler 阻塞 HW | HW 無 shader 可跑 | ISA sim 當 stub 解耦 |
| 人力變動(bus factor) | 時程崩 | Pair review 強制、doc 強制 |
| SystemC → RTL 落差 | Phase 3 爆炸 | Phase 2 coding style 已 synth-friendly |
| CTS/dEQP 最後爆 | 不能宣稱 ES 2.0 | Phase 1 起就跑 conformance |

---

## Part G — Definition of Done

| Phase | DoD |
|---|---|
| 0 | Spec signed off,CI 綠,ISA 表達力驗證通過 |
| 1 | Ref model 過 CTS subset,Compiler 過 shader corpus,TLM 過 scene regression |
| 2 | CA 模型 scene + shader 全綠,perf 達目標,coverage 達門檻 |
| 3 | RTL vs CA equivalence 全綠,synth 收斂,lint / CDC / STA 全綠 |
| 4 | FPGA 跑 real app,dEQP MSAA subset 綠,signoff metrics 達標 |

---

## 總時程

| Phase | 期間 | 月數 |
|---|---|---|
| 0 | M0 – M2.5 | 2.5 |
| 1 | M2.5 – M9 | 6.5 |
| 2 | M9 – M16 | 7.0 |
| 3 | M16 – M23.5 | 7.5 |
| 4 | M23.5 – M28 | 4.5 |
| **Total** | | **28.0** |

**Area budget**(28nm 估算):
- Tile buffer SRAM +48 KB(vs no-MSAA baseline)
- ROP 4× wide:+20–30% ROP area
- Resolve unit:~0.01 mm²
- 整體 die size:+8–12%

**Performance estimate**:
- 1× 場景:peak 1 pixel/cycle
- 4× MSAA 場景:FPS 較 1× 下降 ~15–25%(主要來自 tile buffer 與 ROP bank conflict)
- 相較 4× supersampling(降 75%):大勝
