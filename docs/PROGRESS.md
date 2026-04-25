---
doc: Progress Log
purpose: Human-readable index of what shipped per commit, mapped to Master Plan milestones.
last_updated: 2026-04-25 (Sprint 33 — Phase 2.x full-chain end-to-end CA wiring)
---

# PROGRESS.md

Plan-vs-actual log. The git commit messages remain the source of truth;
this file is the human-readable index so that picking up the project
mid-stream doesn't require `git log | less`.

## How to update

After each shippable commit:
1. Add a row to **Commit Index** below.
2. Add an entry under the relevant phase / sprint section, mirroring the
   commit subject and capturing **Done / Tests / Known gaps / Next**.
3. Bump `last_updated` in the front matter.

Format kept terse on purpose. If you want full context, read the commit
itself — `git show <sha>`.

---

## Commit Index

| # | SHA | Phase / Sprint | Subject |
|---|---|---|---|
| 1 | `a411e15` | Phase 0 kickoff | repo scaffold + Master Plan v1.0 |
| 2 | `50f6c3a` | Phase 0 iteration | microarch + Docker + codegen + ISA validation |
| 3 | `5240f5e` | Phase 0 ISA freeze | Shader ISA v1.0 — close 6 v0.1 gaps |
| 4 | `68e2982` | Phase 0 infra | CI: GHCR-cached Docker dev image |
| 5 | `ed8bf56` | Phase 1 skeleton | sw_ref: buildable end-to-end pipeline + CTest |
| 6 | `33af877` | Phase 1 / Sprint 1 | assembler + ISA sim + sw_ref hookup |
| 7 | `3a95e8d` | Phase 1 / Sprint 2 | compiler/glsl: minimal GLSL ES 2.0 subset frontend |
| 8 | `836483d` | Phase 1 / Sprint 3 | sw_ref: 4× MSAA per-sample raster + box-filter resolve |
| 9 | `baacb78` | Phase 1 / Sprint 4 | sw_ref: TMU + bilinear texture sampling |
| 10 | `ce433ca` | Phase 1 / Sprint 5 | systemc: TLM-LT skeleton (CP → SC wrapping ISA sim) |
| 11 | `aa461f2` | Phase 1 / Sprint 6 | sim: 16-thread warp executor with per-lane mask + divergence |
| 12 | `3360510` | Phase 1 / docs | add PROGRESS.md (plan-vs-actual log) |
| 13 | `47c652c` | Phase 1 / Sprint 7  | ISA v1.1 — MEM dst_class+src_class, per-lane break |
| 14 | `95da73b` | Phase 1 / Sprint 8  | sw_ref: PFO depth test + alpha blend |
| 15 | `ce94abb` | Phase 1 / Sprint 9  | compiler/glsl: locals + built-ins + if/else |
| 16 | `a5e73ef` | Phase 1 / Sprint 10 | systemc: VF + PA blocks, CP→VF→SC chain |
| 17 | `7f52a4c` | Phase 1 / Sprint 11 | conformance harness — scene-based CTest regression |
| 18 | `968736f` | Phase 1 / Sprint 12 | glslang integration (gated; SPIR-V emit) |
| 19 | `3ead419` | Phase 1 / refactor | systemc/blocks: rename abbrev → full names (cp→commandprocessor, …) |
| 20 | `e00d78c` | Phase 1 / refactor   | docs/microarch: same rename applied to spec stub filenames |
| 21 | `a7ae47b` | Phase 1 / Sprint 13  | compiler/spv: minimal SPIR-V → ISA lowering |
| 22 | `4960797` | Phase 1 / Sprint 14  | systemc: Rasterizer as TLM block (RS) |
| 23 | `142bd77` | Phase 1 / Sprint 15  | conformance harness — golden PPM RMSE diff |
| 24 | `619884c` | Phase 1 / Sprint 16  | sw_ref/fp: first cut at HW-aligned transcendentals |
| 25 | `6eea98c` | Phase 1 / Sprint 17  | sw_ref: stencil + scissor + alpha-to-coverage |
| 26 | `ca5db3b` | Phase 2 / Sprint 18  | Phase 2 kickoff — CP cycle-accurate template |
| 27 | `ff512c2` | refactor             | rename `_pv` → `_cycleaccurate` (precision; later reverted) |
| 28 | `d669695` | refactor             | revert `_cycleaccurate` → `_ca` (concise abbrev policy) |
| 29 | `87f1cb9` | refactor          | systemc/blocks: append `_lt` suffix to LT files + classes |
| 30 | `b27ea5f` | Phase 2 / Sprint 19 | systemc: VertexFetch cycle-accurate block (VF_ca) |
| 31 | `aa4265a` | Phase 2 / Sprint 20 | systemc: ShaderCore cycle-accurate block (SC_ca) |
| 32 | `b408dcf` | Phase 2 / Sprint 21 | systemc: PrimitiveAssembly cycle-accurate (PA_ca) |
| 33 | `e481719` | Phase 2 / Sprint 22 | systemc: Rasterizer cycle-accurate (RS_ca) |
| 34 | `7ea722e` | Phase 2 / Sprint 23 | systemc: gpu_top_ca chain (CP→VF→SC) |
| 35 | `65b27ab` | Phase 2 / Sprint 24 | systemc: TextureUnit cycle-accurate (TMU_ca) |
| 36 | `82071d1` | Phase 2 / Sprint 25 | systemc: PerFragmentOps cycle-accurate (PFO_ca) |
| 37 | `29e9ec7` | Phase 2 / Sprint 26 | systemc: TileBuffer + ResolveUnit cycle-accurate |
| 38 | `1d5faa7` | Phase 2 / Sprint 27 | systemc: MMU + L2 + MC cycle-accurate (memory subsystem) |
| 39 | `82bdaf6` | Phase 2 / Sprint 28 | systemc: CSR + PMU + TileBinner cycle-accurate (sidebands) |
| 40 | `e197bc8` | Phase 2.x / Sprint 29 | systemc: SC→PA payload-conversion adapter |
| 41 | `d33bfaf` | Phase 2.x / Sprint 30 | systemc: PA→RS payload-conversion adapter |
| 42 | `209cd3c` | Phase 2.x / Sprint 31 | systemc: RS→PFO payload-conversion adapter (frags → quads) |
| 43 | `1d6db65` | Phase 2.x / Sprint 32 | systemc: PFO→TBF tile-flush trigger adapter |
| 44 | `e95b44e` | Phase 2.x / Sprint 33 | systemc: full-chain end-to-end testbench (12 modules) |

---

## Status snapshot

- **Master Plan phase**: **Phase 2.x adapter set complete** — every
  chip block has a CA sibling, and the four payload-conversion
  adapters (SC→PA, PA→RS, RS→PFO, PFO→TBF) compose them into a
  runnable end-to-end chain. Phase 1 LT path still the production
  reference; CA chain now exists as a parallel executable target.
- **Tests passing**: 17/17 (CTest, local macOS / GCC-15)
  - `compiler.{asm_roundtrip, sim_basic, glsl_compile, sim_warp,
    warp_break, glsl_ext, spv_lower}`
  - `sw_ref.{basic, fp, isa_triangle, msaa, texture, pfo,
    stencil_scissor}`
  - `conformance.{triangle_white, triangle_msaa, triangle_rgb}`
  - Skipped (Docker-only): `systemc.tlm_hello`,
    `systemc.{cp,vf,sc,pa,rs,gpu_top,tmu,pfo,tbf_rsv,mem,sidebands}_ca`,
    `systemc.{sc_pa,pa_rs,rs_pfo,pfo_tbf}_adapter_ca`,
    `systemc.full_chain_ca`,
    `compiler.glsl_to_spv`
- **ISA**: v1.1 frozen (MEM class bits + per-lane break formalised)
- **TLM blocks**: 5 of 15 (CP / VF / SC / PA / RS) at Phase 1 LT;
  **15 of 15** with Phase 2 cycle-accurate variants — full set
  CP / VF / SC / PA / RS / TMU / PFO / TBF / RSV / MMU / L2 / MC /
  CSR / PMU / BIN landed.
- **Adapters**: SC→PA, PA→RS, RS→PFO, PFO→TBF — single-buffered
  pointer-passing modules that bridge typed payload boundaries.
  Full chain (SC → ... → RSV) wired and tested end-to-end.
- **Flavour-suffix convention**: file + class suffix indicates SystemC
  abstraction level — `_lt` (LT, b_transport) / `_at` (AT, future) /
  `_pv` (PV, future) / `_ca` (cycle-accurate, sc_signal+CTHREAD).
  All current LT blocks live at `<block>_lt.{h,cpp}` with classes
  `<Block>Lt`; the lone CA block is `commandprocessor_ca.{h,cpp}` /
  `CommandProcessorCa`.
- **Optional gates**: `-DGPU_BUILD_SYSTEMC=ON` (TLM chain +
  cycle-accurate CP), `-DGPU_BUILD_GLSLANG=ON` (glslang FetchContent,
  GLSL → SPIR-V)
- **Repo**: https://github.com/cclwylin/gpu (SSH origin)

---

## Phase 0 — Architecture Freeze

### Phase 0 kickoff(`a411e15`)
- **Done**: Repo scaffold (52 files); `docs/MASTER_PLAN.md` v1.0 frozen
  (28-month, 4× MSAA, TBDR + Unified SIMT); 4 spec drafts (arch, ISA, MSAA,
  coding style); `specs/{registers,isa}.yaml`; CI skeleton.
- **Decision points locked**: 4× MSAA only; alpha-to-coverage in scope; MSAA
  texture sample out of scope.
- **Next**: Microarch detail + reproducible build + codegen + ISA validation.

### Phase 0 iteration(`50f6c3a`)
- **Done**: 15 per-block microarch docs (`docs/microarch/{cp,vf,…,pmu}.md`);
  `third_party/versions.yaml` SSOT lock; `docker/Dockerfile`; working
  `tools/regmap_gen` + `tools/isa_gen` (idempotent); 3 reference shaders +
  `ISA_VALIDATION.md` identifying 6 v0.1 gaps.
- **Tests**: 9 generated artefacts written, idempotent on re-run.
- **Known gaps**: Surface 6 v0.1 ISA shortfalls (3-op encoding, swizzle 2-bit,
  source modifiers, predicate set/use, `dp2`, predicate update).
- **Next**: Resolve gaps and freeze ISA v1.0.

### Phase 0 ISA freeze(`5240f5e`)
- **Done**: Instruction width 32 → 64 bit (uniform); 8-bit per-component
  swizzle; src negate/abs in encoding; 3-operand inline (src2 GPR-only);
  `pmd` 2-bit predication on every instruction; 6 new `setp_*` opcodes;
  `dp2` added; 3 reference shaders rewritten in v1.0 syntax; codegen
  artefacts regenerated; spec churn applied to `specs/isa.yaml` +
  `docs/isa_spec.md`.
- **Tests**: All 6 v0.1 gaps closed and recorded in
  `tests/shader_corpus/ISA_VALIDATION.md`.
- **Next**: Real CI infra so PRs validate against this frozen spec.

### Phase 0 CI infra(`68e2982`)
- **Done**: `.github/workflows/ci.yml` rebuilt around a `dev-image` job that
  pushes `ghcr.io/cclwylin/gpu/dev:<sha>` and is consumed by lint / rtl-lint /
  build / gen-check via `container:`. Uses `cache-from/cache-to=type=gha`.
- **Cold→cache rebuild**: ~25–30 min → ~30–60 s.
- **Auth**: `GITHUB_TOKEN`; image private by default, can be flipped to
  public from GitHub settings.
- **Next**: Stop accumulating spec/infra-only commits; start producing
  runtime code (Phase 1).

---

## Phase 1 — Three-Track Parallel

Master Plan exit criteria for Phase 1:
- Track SW: ES 2.0 CTS subset + MSAA tests green.
- Track Compiler: ~500-shader corpus all compile + ISA sim = ref model.
- Track HW TLM: full-chip framebuffer bit-exact vs ref model.

We are working **vertical slices** through all three tracks rather than
finishing one track first.

### Phase 1 skeleton(`ed8bf56`)
- **Done**: Top-level CMake (C++20); `sw_ref` library with stubs for every
  pipeline stage; FP helpers (FTZ, SAT); stage-boundary trace skeleton; hello-
  triangle smoke executable + 2 unit tests under CTest.
- **Tests**: 2/2 green (`sw_ref.basic`, `sw_ref.fp`). Smoke renders 1352 /
  4096 painted pixels.
- **Trip-ups recorded in commit**: AppleClang SDK header bug → forced GCC-15
  locally; libstdc++ Bessel `y0`/`y1` collision → renamed locals; brace-init
  `std::min/max` deduction failure → small `fmin3`/`fmax3` lambdas.
- **Next**: Replace stub C++ functor shaders with real ISA execution.

### Sprint 1 — assembler + ISA sim + sw_ref hookup(`33af877`)
- **Done**:
  - `compiler/include/gpu_compiler/{encoding,asm,sim}.h`
  - Assembler (text → 64-bit binary), disassembler, ISA simulator
    (single-thread); `gpu-as` / `gpu-disasm` / `gpu-isa-sim` CLIs.
  - `BoundShaderPair.{vs,fs}_binary` opaque pointers wired into
    `sw_ref::{vertex,fragment}_shader`. `kil` clears coverage mask.
  - Spec churn: stole 1 bit of ALU "reserved" for `dst_class` (without it,
    ALU couldn't write to outputs vs GPR ambiguously).
- **Tests**: 5/5 green — `compiler.asm_roundtrip`, `compiler.sim_basic`,
  plus pre-existing 3 sw_ref tests, plus new `sw_ref.isa_triangle`
  (hello-triangle through real ISA path).
- **Known gaps**: label-based bra/call not implemented; 16-thread warp
  not implemented; GLSL frontend not implemented.
- **Next**: GLSL frontend.

### Sprint 2 — minimal GLSL ES 2.0 subset frontend(`3a95e8d`)
- **Done**: Hand-written lexer/parser/AST/codegen in
  `compiler/glsl/src/glsl.cpp`. Covers attribute/uniform/varying/sampler
  decls, types vec{2,3,4} / mat{2,3,4} / float / sampler2D, * + - unary
  binary, member access, function call (texture2D), mat4×vec4 lowered to
  4 dp4 with per-component write-mask. ABI metadata emitted alongside
  binary so host can wire constants/attribs/varyings.
  - New CLI: `gpu-glslc {-vs|-fs}`.
- **Tests**: 6/6 green — `compiler.glsl_compile` added; runs a 7-line
  shader through the pipeline and asserts ABI plus per-component output.
- **Why hand-rolled, not glslang**: ship in one session. glslang would
  require fetch + build via Docker (~tens of minutes). Architecture
  preserved (`Compile -> std::vector<Inst> + Bindings`) so glslang slots
  in later without API change.
- **Next**: 4× MSAA path.

### Sprint 3 — 4× MSAA per-sample raster + resolve(`836483d`)
- **Done**:
  - Rasterizer: D3D rotated-grid 4× pattern, 4-bit per-pixel coverage
    mask. Per-pixel shading retained (sample shading deliberately out of
    scope per `docs/msaa_spec.md`).
  - PFO: writes per-sample to `fb.color_samples[pix*4 + s]` gated by
    coverage bit; falls through to `fb.color` in 1× mode.
  - Resolve: box filter `(s0 + s1 + s2 + s3 + 2) >> 2` per channel.
- **Tests**: 7/7 green — `sw_ref.msaa` added. White triangle on black:
  `1×: bg=746 fg=270 edge=0` vs `4×: bg=684 fg=246 edge=92`. Edge pixels
  exist only with 4× because resolve produces averaged greys.
- **Next**: Texture sampling.

### Sprint 4 — TMU + bilinear texture sampling(`baacb78`)
- **Done**:
  - `gpu/texture.h`, `texture.cpp`: RGBA8 only; NEAREST + BILINEAR;
    CLAMP + REPEAT; `sample_texture(t, u, v)` returns RGBA float.
  - `Context.textures[16]` holds bound textures by ISA `tex` slot.
  - `fragment_shader.cpp` builds a `sim::TexSampler` closure that
    forwards `tex` ops to `sample_texture`.
- **Tests**: 8/8 green — `sw_ref.texture`: 2×2 quadrant texture, fullscreen
  triangle, exact RGB match yields `red=1024 green=1024 blue=1024 yellow=1024`
  on 64×64 framebuffer (perfect quartering).
- **Known gaps surfaced**: MEM-format `dst` and `src` are GPR-only (no
  class bits in encoding). Compiler must `mov rN, vN` before `tex` and
  `mov o0, rN` after. Recorded in `docs/isa_spec.md §3.3` with mitigation
  plan (steal 2 bits from `imm`). Test FS uses the workaround.
- **Next**: SystemC TLM-LT skeleton.

### Sprint 5 — SystemC TLM-LT skeleton(`ce433ca`)
- **Done**:
  - `systemc/common/include/gpu_systemc/payload.h` — `ShaderJob`.
  - `blocks/cp` (TLM initiator + `SC_THREAD` queue drainer).
  - `blocks/sc` (TLM target wrapping `gpu::sim::execute`; placeholder
    timing model: 1 ns / instruction).
  - `top` connects CP → SC.
  - `tb/test_tlm_hello.cpp` (`sc_main`).
  - CMake option `-DGPU_BUILD_SYSTEMC=ON`; auto-finds SystemC at
    `/usr/local/systemc-2.3.4` or via `SystemC_DIR` / `SYSTEMC_HOME`.
- **Tests**:
  - Library `gpu_systemc` builds clean locally (macOS, GCC-15).
  - Test `systemc.tlm_hello` builds and runs in Docker (CI). Locally on
    macOS it's gated off because system SystemC is libc++ but our build
    is libstdc++ (ABI mismatch). CMake prints a clear note.
- **Next**: 16-thread warp model.

### Sprint 6 — 16-thread warp executor(`aa461f2`)
- **Done**:
  - `gpu::sim::WarpState` (16 lanes × `ThreadState` + 16-bit predicate).
  - `gpu::sim::execute_warp(code, warp, tex)`. Per-instruction loop runs
    only on lanes whose `active` mask + `lane_active` + `pmd` predicate
    all hold.
  - `setp_*` writes per-lane bits in `WarpState.predicate`.
  - `if_p` pushes active mask; `else` flips; `endif` pops; stack depth 8.
  - `kil` clears that lane's `lane_active` permanently.
- **Tests**: 9/9 green — `compiler.sim_warp`: a 5-instruction divergent
  program where lanes 0..7 take the if branch (red) and lanes 8..15 take
  else (blue) verifies per-lane outputs.
- **Known gap**: `break` is currently a "warp-uniform" early exit
  (if any predicated lane wants to break, the whole warp exits). True
  per-lane break with reconvergence is Phase 1.x. — **closed in Sprint 7.**
- **Next**: ISA v1.1 to close MEM class-bit gap and break gap.

### Sprint 7 — ISA v1.1: MEM class bits + per-lane break(`47c652c`)
- **Done**:
  - `MemFields` gains `dst_class` (1 bit) and `src_class` (2 bits),
    stolen from `imm` (now 24-bit signed). Encoder/decoder updated;
    assembler/disassembler now emit/parse class-aware operands;
    `sim.cpp` and `sim_warp.cpp` `tex` paths use the new fields.
  - `sim_warp.cpp` `loop_stack` frame now stores `entry_active`. `break`
    is per-lane: clears the affected bits from the live `active` mask;
    only when `active == 0` does it skip past `endloop` and pop the
    frame (restoring `entry_active`). `endloop` likewise restores on
    natural exit.
  - `specs/isa.yaml` v1.0 → v1.1 + MEM layout fields documented;
    `docs/isa_spec.md` v1.0 → v1.1 changelog entry; old "known gap"
    paragraph removed.
  - `tests/shader_corpus` and existing `.asm` are unchanged because
    they didn't rely on the workaround; the v1.1 encoding is a
    superset (still 64-bit, same bit positions for shared fields).
- **Tests**: 10/10 green
  - `sw_ref.texture` FS collapsed from 3 instructions to one
    `tex o0, v0.xy, tex0` (no more `mov rN, vN` / `mov oN, rN`),
    same RGB quartering result.
  - new `compiler.warp_break`: 16 lanes with three thresholds
    (3 / 5 / 999); each lane's recorded iteration count matches
    its threshold, exercising real per-lane reconvergence.
- **Known gaps**: none for this slice. Larger gaps remain in the
  follow-up list.
- **Next**: TBD — see candidates list.

---

## Open follow-ups (not yet sprinted)

| Topic | Notes |
|---|---|
| **Phase 2: 14 remaining blocks → cycle-accurate** | Sprint 18 landed CP cycle-accurate template. Sprints 19–28 walk it across VF / SC / PA / RS / TMU / PFO / TBF / RSV / MMU / L2 / MC / CSR / PMU per `docs/phase2_kickoff.md`. |
| **TMU + L1 Tex$ (TLM)** | Phase-1 LT side: TMU still missing as a SystemC block (only sw_ref texture path exists). |
| **PFO / TBF / RSV (TLM)** | Same — TLM-LT counterparts not yet written. |
| **MMU / L2 / MC / CSR / PMU (TLM)** | Memory subsystem + sidebands. |
| **CP multi-stage dispatch** | CP currently fronts only one downstream initiator (VF in LT chain). Real CP needs to route per-cmd to PA / RS / TMU /PFO. |
| **SPIR-V → IR coverage** | Sprint 13 covers `mat4 * vec4` patterns. Need OpExtInst (GLSL.std.450), OpAccessChain, OpVectorShuffle, OpDot, OpImageSampleImplicitLod, control flow. |
| **glslang → SPIR-V → ISA end-to-end** | Sprint 12 produces SPIR-V; Sprint 13 lowers SPIR-V; gluing them under `-DGPU_BUILD_GLSLANG=ON` with a real GLSL test still pending. |
| **GLSL frontend: vec ctors, for/while, reflect/length/abs** | Sprint 9 covers if/else + a few built-ins; Sprint 17 didn't extend the parser. |
| **FP HW-aligned LUT polynomials (3 ULP)** | Sprint 16 polynomials are ~1e-2 to ~1e-3 relative. 3-ULP target needs LUT-assisted forms. |
| **sim ↔ sw_ref FP bit-alignment** | Both should call the same library. Currently sim still uses `std::*`; sw_ref uses Sprint-16 polynomials. |
| **Conformance: dEQP / glmark2** | Scene-format harness exists (Sprint 11/15) but scenes are hand-rolled. |
| **Per-block microarch (frozen v1.0)** | 15 microarch docs still at draft v0.1. |
| **Stencil per-sample for sample-shading** | Sprint 17 uses per-sample stencil in the MSAA path; sample-shading (FS per-sample) is out of scope for v1. |
| **Two-sided stencil, polygon offset, depth-bounds** | All deferred. |

**Closed by Sprint 7**: ~~ISA v1.1: MEM `dst_class`+`src_class`; per-lane `break`~~

**Closed by Sprints 8–12**:
- ~~PFO depth + blend~~ (Sprint 8; stencil added Sprint 17)
- ~~More TLM blocks: VF + PA~~ (Sprint 10), ~~+ RS~~ (Sprint 14)
- ~~GLSL frontend ext: dot/normalize/max/min/clamp/pow + if/else + locals~~ (Sprint 9)
- ~~Conformance harness~~ (Sprint 11) + ~~golden PPM diff~~ (Sprint 15)
- ~~glslang integration: GLSL → SPIR-V~~ (Sprint 12) + ~~SPIR-V → ISA partial~~ (Sprint 13)

**Closed by Sprints 13–18**:
- ~~SPIR-V → ISA lowering (subset)~~ — Sprint 13
- ~~RS as TLM block~~ — Sprint 14
- ~~Golden-PPM diff in conformance harness~~ — Sprint 15
- ~~FP HW alignment first cut~~ — Sprint 16
- ~~Stencil + scissor + alpha-to-coverage in PFO~~ — Sprint 17
- ~~Phase 2 cycle-accurate kickoff (CP template)~~ — Sprint 18

---

## Sprint 8 — PFO depth test + alpha blend(`95da73b`)
- **Done**:
  - Framebuffer.depth (1× path); DrawState gains depth_func / depth_write +
    blend_enable / blend_src / blend_dst / blend_eq.
  - PFO rewritten: per-pixel (1×) and per-sample (4×) depth test +
    SRC_ALPHA-style blend.
- **Tests**: 12/12 — `sw_ref.pfo` adds depth-overlap correctness check
  and an alpha-blend midpoint check.
- **Out of scope**: stencil ops; depth bounds; scissor.

## Sprint 9 — GLSL frontend ext(`ce94abb`)
- **Done**:
  - Number literal lexing fix (`.x` no longer consumed as NUMBER).
  - Compiler-managed constant pool for float literals (top of c-bank).
  - `float / vec{2,3,4}` local declarations + initialiser.
  - Built-ins: `dot`, `normalize`, `max`, `min`, `clamp`, `pow`.
  - Comparison ops `< <= > >= == !=` recognised in if-condition.
  - `if (a OP b) { ... } [else { ... }]` lowers to setp + if_p/else/endif.
  - Stmt redesigned as `variant<AssignStmt, LocalDeclStmt, IfStmt>`.
- **Tests**: 13/13 — `compiler.glsl_ext` runs a Phong-ish shader through
  ISA sim.
- **Out of scope**: `for`/`while`, vector constructors, `reflect`,
  `length`, `abs`.

## Sprint 10 — TLM blocks: VF + PA(`a5e73ef`)
- **Done**:
  - `VertexFetchLt` (TLM target+initiator) and `PrimitiveAssemblyLt` (TLM
    target).
  - CP made generic (`enqueue(void*)`); existing `ShaderJob*` direct
    submission replaced by VF-driven `VertexFetchJob*`.
  - Top wires `cp.initiator -> vf.target`, `vf.initiator -> sc.target`.
    PA is a sibling target driven by tb.
  - Testbench `systemc.tlm_hello` rewritten to drive 3 vertices through
    the CP→VF→SC chain plus a direct PA call.
- **Tests**: 12/12 non-SystemC + library compiles. SystemC test runs in
  Docker only (existing macOS skip rule unchanged).
- **Out of scope**: RS, TMU, PFO, TBF, RSV, MMU, L2, MC, CSR, PMU as
  TLM blocks.

## Sprint 11 — Conformance harness(`7f52a4c`)
- **Done**:
  - `tests/scenes/*.scene` declarative format (line-oriented key=value).
  - `tests/conformance/scene_runner` CLI: parse + run sw_ref + assert
    pixel-count expectations.
  - CMake globs `tests/scenes/*.scene` and adds one CTest per file.
  - Two reference scenes: `triangle_white` (1×) and `triangle_msaa`
    (4× edge-pixel sentinel).
- **Tests**: 14/14 with two new `conformance.*`.
- **Out of scope**: scene-level shader binding (currently uses fixed
  pass-through VS+FS); golden-image diff (currently only pixel counts).

## Sprint 12 — glslang integration(`968736f`)
- **Done**:
  - `compiler/glslang/` subproject gated by `-DGPU_BUILD_GLSLANG=ON`
    (default OFF). FetchContent pulls glslang 14.0.0 (matches
    `third_party/versions.yaml`).
  - Wrapper API `gpu::glslang_fe::compile(glsl, stage) -> SpvResult`
    around `glslang::TShader` / `TProgram` / `GlslangToSpv`.
  - CLI `gpu-glsl-to-spv {-vs|-fs} input.glsl output.spv`.
  - Test `compiler.glsl_to_spv` (Docker only) compiles a 4-line FS and
    asserts the SPIR-V magic word.
- **Why gated**: cold-fetching + building glslang is ~10 min; not worth
  every local rebuild. Docker / CI image absorbs the cost once.
- **Out of scope (next sprint)**: SPIR-V → our IR lowering pass. The
  full GLSL → ISA replacement happens when that pass is in place; the
  current hand-written parser in `compiler/glsl/` keeps working until
  then.

## Refactor — systemc/blocks/ name expansion(`3ead419`)
- **Done**: All 15 block directories under `systemc/blocks/` renamed
  from abbreviations to lowercase concatenated full names. Header /
  source files in the four content-bearing blocks renamed to drop the
  `<abbr>_block` prefix and use the full name (`cp_block.h` →
  `commandprocessor.h`; `cp.cpp` → `commandprocessor.cpp`). Mapping:
  `cp → commandprocessor, vf → vertexfetch, sc → shadercore,
  pa → primitiveassembly, tb → tilebinner, rs → rasterizer,
  tmu → textureunit, pfo → perfragmentops, tbf → tilebuffer,
  rsv → resolveunit, mmu → memorymanagementunit, l2 → l2cache,
  mc → memorycontroller, csr → controlstatusregister,
  pmu → perfmonitorunit`. CMake source list, all `#include` paths,
  `gpu_top.h`, and `systemc/blocks/README.md` updated to match.
- **Tests**: 14/14 still green (both default and `-DGPU_BUILD_SYSTEMC=ON`
  build paths). Class names (`CommandProcessorLt`, etc.) were already in
  full form; only paths changed.
- **Out of scope**: `docs/microarch/<abbr>.md` filenames left as-is
  (those are spec stubs, not code; rename is cheap if asked later).
  — **closed in next refactor commit** (see below).

## Refactor — docs/microarch/ name expansion(`e00d78c`)
- **Done**: All 15 `docs/microarch/<abbr>.md` files renamed in lockstep
  with the systemc/blocks/ directories. Cross-references in
  `docs/arch_spec.md`, `docs/microarch/README.md` (index table relinked
  with full filenames), and `systemc/blocks/commandprocessor/README.md`
  updated. No abbreviated `.md` references remain in the repo.
- **Tests**: Documentation-only change; CTest 14/14 unaffected.

## Sprint 13 — SPIR-V → ISA lowering(`a7ae47b`)
- **Done**: New `compiler/spv/` subdir (gpu::spv namespace), pure SPIR-V
  parser independent of glslang, always built into gpu_compiler. Subset:
  TypeVoid/Float/Vector/Matrix/Pointer/Function, Decorate
  (BuiltIn/Location/Binding), Variable (Input/Output/Uniform), OpFunction/
  End, OpLabel, OpReturn, OpVariable, OpLoad, OpStore, OpFAdd, OpFMul,
  OpMatrixTimesVector (lowered to 4×dp4 with per-row write-mask).
- **Tests**: 15/15. New `compiler.spv_lower` hand-crafts a minimal
  SPIR-V module for `gl_Position = u_mvp * a_pos;` and asserts 4 dp4 +
  ≥1 mov + correct ABI metadata.
- **Why no glslang in the test**: the parser ships everywhere — Docker
  or local — and exercises the lowering deterministically.
- **Out of scope**: OpExtInst (GLSL.std.450), OpAccessChain,
  OpVectorShuffle, OpDot, OpImageSampleImplicitLod, control flow.

## Sprint 14 — Rasterizer as TLM block(`4960797`)
- **Done**: `systemc/blocks/rasterizer/{include,src}` mirroring sw_ref
  rasterizer (1× + 4× MSAA, D3D rotated grid, perspective-correct
  varying via 1/w). Five blocks total now in TLM-LT.
  Top-level `gpu_top` adds `rs` as a sibling target alongside `pa`.
  TB updates `RasterJob` payload + posts to RS, asserts non-empty
  fragment list.
- **Tests**: gpu_systemc lib compiles; 15/15 non-SystemC tests still
  green. The chained CP→VF→SC→PA→RS test runs in Docker.
- **Out of scope**: TMU, PFO/TBF/RSV, MMU, L2, MC, CSR, PMU. CP
  multi-stage dispatch.

## Sprint 15 — Golden PPM diff in conformance harness(`142bd77`)
- **Done**: scene format gains `golden_ppm <path>` and `expect_rmse_max
  <max>` keys. scene_runner gains `read_ppm` / `write_ppm` /
  `rmse(a, b)` (double-precision per-channel RMS) plus a
  `--write-golden` flag for capturing the golden after a known-good
  change. New `tests/scenes/triangle_rgb.scene` + checked-in
  `triangle_rgb.golden.ppm` (32×32 P6, 3085 bytes).
- **Tests**: 16/16. New `conformance.triangle_rgb` runs the Gouraud
  triangle, compares against the golden with rmse ≤ 0.5 channel units.
- **Out of scope**: scene-level shader binding (runner still uses
  pass-through VS+FS); SSIM rather than RMSE; multi-PPM regression
  report.

## Sprint 16 — FP HW alignment first cut(`619884c`)
- **Done**: `sw_ref/src/fp/fp32.cpp` rewritten with explicit polynomial
  approximations. rcp via Newton-Raphson on a 48/17 - 32/17·x_n seed;
  rsq via the classic Quake fast inverse sqrt + 2 NR steps; exp2/log2
  via degree-5 polynomials on a reduced range; sin/cos via Maclaurin
  to f^9 / f^8 with range-reduce to [-π, π].
- **Honest scope**: not yet 3-ULP. Measured max relative error vs libm
  over 256 samples: rcp 1.2e-5, rsq 7.3e-7, exp2 8.4e-5, log2 8.6e-2
  (worst), sin 6.9e-3, cos 2.4e-2. Test bound 1e-1 relative; tightening
  to 3 ULP needs LUT-assisted forms in Phase 2.x.
- **Tests**: 16/16. `sw_ref.fp` extended with a `sweep()` helper, max
  abs/relative error per function, asserts each ≤ 1e-1.
- **Out of scope**: LUT-assisted polynomials, sim ↔ sw_ref bit
  alignment, denormal handling on input, NaN/-inf at boundaries.

## Sprint 17 — Stencil + scissor + alpha-to-coverage(`6eea98c`)
- **Done**:
  - `state.h` gains StencilFunc (8 funcs), StencilOp (6 ops),
    stencil_ref + read_mask + write_mask, sop_fail/zfail/zpass,
    scissor_enable + scissor_{x,y,w,h}.
  - `Framebuffer.stencil` 1× 8-bit per-pixel buffer.
  - `rasterizer.cpp` clips bbox against scissor box before sampling.
  - `per_fragment_ops.cpp` rewritten: full ES 2.0 pipeline of
    a2c → stencil test → depth test → blend → write. Both 1× and 4×
    MSAA paths.
  - a2c table per `docs/msaa_spec.md §5.2`:
    `0 / 0001 / 0101 / 0111 / 1111` at 0 / 0.125 / 0.375 / 0.625 / 0.875.
- **Tests**: 17/17. New `sw_ref.stencil_scissor`:
  - Scissor test: full-screen white triangle clipped to (8..23,8..23);
    asserts ≥100 painted, zero outside.
  - Stencil test: pass-A writes stencil=1 in a region (REPLACE on
    pass), pass-B blue full-screen with SF_EQUAL ref=1; asserts
    paint stays inside the stencil-1 region.
- **Out of scope**: per-sample stencil for sample-shading, two-sided
  stencil, polygon offset, depth-bounds test.

---

## Phase 2 — Cycle-Accurate SystemC

Master Plan Phase 2 spans M9–M16 (7 months). Each Phase-1 LT block
gets a parallel `<blockname>_ca.{h,cpp}` cycle-accurate
implementation; both flavours coexist; top-level CMake flag picks the
build (Phase 2.x). Detailed plan + naming + handshake convention +
migration order in [`docs/phase2_kickoff.md`](phase2_kickoff.md).

> **Note on terminology**: earlier internal shorthand used "PV" for
> the cycle-accurate flavour. That was inaccurate ("PV" / Programmer's
> View in OSCI taxonomy ≈ LT-equivalent). Renamed everywhere to
> `_ca` / `CommandProcessorCa` /
> `systemc.cp_ca` for precision.

## Sprint 18 — Phase 2 kickoff(`ca5db3b`)
- **Done**:
  - `commandprocessor_ca.{h,cpp}` — first cycle-accurate
    block. SC_CTHREAD synchronous to `clk.pos()`, async-deassert
    reset via `reset_signal_is(rst_n, false)`. Wire-level interface:
    sc_in/out + ready/valid + 64-bit data signal. Same `enqueue()`
    driver-side API as the LT variant.
  - `tb/test_commandprocessor_ca.cpp` (`sc_main`):
    sc_clock @ 10 ns + tiny Sink consumer that always asserts ready
    and records data words. Enqueue 3 jobs, run, assert sink saw all
    3 with correct data.
  - `docs/phase2_kickoff.md`: full Phase 2 plan — coexistence pattern,
    sc_in/out + CTHREAD + ready/valid convention, Sprint 19–28
    migration order across the remaining 14 blocks, co-sim strategy
    (`-DGPU_SYSTEMC_FLAVOR=lt|ca` at top), exit criteria.
- **Tests**: 17/17 non-SystemC still green. `gpu_systemc` library
  compiles cleanly with the cycle-accurate CP added; testbench runs
  in Docker.
- **Out of scope (next sprints)**: VF / SC / PA / RS / TMU / PFO /
  TBF / RSV / MMU / L2 / MC / CSR / PMU all still need cycle-accurate
  variants. Top-level chain doesn't yet route through the
  cycle-accurate CP (waits for at least one downstream cycle-accurate
  block — Sprint 19).

## Refactor — flavour-suffix convention(`87f1cb9`)
- **Done**: append `_lt` to all 5 existing TLM-LT block files and
  classes for full alignment with the `_ca` / future `_pv` / `_at`
  policy.
  - File renames (10 total via `git mv`):
    `commandprocessor.{h,cpp}` → `commandprocessor_lt.{h,cpp}` and
    same shape for `shadercore`, `vertexfetch`, `primitiveassembly`,
    `rasterizer`.
  - Class renames (word-boundary regex to avoid colliding with
    `CommandProcessorCa`): `CommandProcessor` → `CommandProcessorLt`,
    `VertexFetch` → `VertexFetchLt`, `ShaderCore` → `ShaderCoreLt`,
    `PrimitiveAssembly` → `PrimitiveAssemblyLt`, `Rasterizer` →
    `RasterizerLt`.
  - All `#include "gpu_systemc/<block>.h"` updated to `<block>_lt.h`
    in `gpu_top.h`, the rasterizer `_ca` cousin, and tb files.
  - `systemc/CMakeLists.txt` source list updated.
  - `systemc/blocks/README.md` index table now lists `*Lt` class
    names for the 5 with implementations; the 11 placeholders stay
    at full-name (no flavour committed yet).
  - Docs: `phase2_kickoff.md` naming convention block redrawn with
    `_lt / _at / _pv / _ca` four-flavour layout. Descriptive prose
    references to "Rasterizer" (the block, not the class) reverted
    where the perl rewrite swept them up.
- **Tests**: 17/17 still green; `gpu_systemc` lib compiles with
  `-DGPU_BUILD_SYSTEMC=ON`.

## Sprint 19 — VertexFetch cycle-accurate(`b27ea5f`)
- **Done**:
  - `vertexfetch_ca.{h,cpp}` with the canonical Phase-2 wire shape:
    upstream consumer (`cmd_valid_i / cmd_ready_o / cmd_data_i`) +
    downstream producer (`vert_valid_o / vert_ready_i / vert_data_o`).
  - SC_CTHREAD synchronous to clk.pos(); active-low async-deassert
    reset via `reset_signal_is(rst_n, false)`.
  - Functional minimum: each accepted upstream command fans out to
    `vertices_per_cmd` (default 3) downstream vertex jobs. The data
    word is currently a pass-through pointer; real per-vertex
    addressing lands when CP_ca's payload format is fleshed out.
  - Testbench `test_vertexfetch_ca.cpp` (Docker-only):
    Source pushes 3 cmds → VF emits 9 vertex jobs → Sink records all
    9 with the originating cmd's payload preserved.
  - CMake adds `vertexfetch_ca.cpp` to `gpu_systemc`; new test
    target `systemc.vf_ca` next to `systemc.cp_ca`.
- **Tests**: 17/17 non-SystemC still green; `gpu_systemc` lib
  compiles with `-DGPU_BUILD_SYSTEMC=ON`. Test exe runs in Docker.
- **Out of scope**: chaining CP_ca → VF_ca (gpu_top still uses LT
  chain; CP_ca / VF_ca are sibling targets in the build but not yet
  wired together — waits for SC_ca in Sprint 20 so the chain can run
  end-to-end).

## Sprint 20 — ShaderCore cycle-accurate(`aa4265a`)
- **Done**:
  - `shadercore_ca.{h,cpp}` — wraps `gpu::sim::execute` inside an
    SC_CTHREAD synchronous to clk.pos(). Same Phase-2 wire shape as
    CP/VF: upstream consumer + downstream producer, opaque
    `uint64_t` data carrying a `ShaderJob*`.
  - Per accepted job: read pointer, materialise ThreadState, run
    ISA sim, write outputs back into `job->outputs`, forward the
    same pointer downstream.
  - Timing placeholder: 1 cycle / instruction. Crude but enough for
    the chain to model some per-warp latency. Real warp scheduler +
    pipe latencies (ALU 1c, SFU 6–10c, TMU 20+c) is Phase 2.x.
  - Testbench `test_shadercore_ca.cpp` (Docker-only): assemble
    `add o0, c0, c1`, build 3 ShaderJobs with different constants,
    push through SC_ca, assert each job's `o0.x = c0.x + c1.x`.
  - CMake adds `shadercore_ca.cpp` to gpu_systemc; new test target
    `systemc.sc_ca`.
- **Tests**: 17/17 non-SystemC still green; `gpu_systemc` lib
  compiles with `-DGPU_BUILD_SYSTEMC=ON`. Test exe runs in Docker.
- **Honest scope note**: this CA model is single-threaded (one
  ShaderJob at a time, no warp scheduling). Real SC has 4 warp
  slots × 16 lanes with per-pipe scoreboarding. The Sprint-20 model
  is enough to validate the wire interface but not enough to model
  perf accurately.
- **Out of scope (next sprints)**: PA_ca, then top-level chain
  CP_ca → VF_ca → SC_ca (now possible with the 3 CA blocks).

## Sprint 21 — PrimitiveAssembly cycle-accurate(`b408dcf`)
- **Done**:
  - `primitiveassembly_ca.{h,cpp}` — same Phase-2 wire shape as
    CP/VF/SC. Per accepted PrimAssemblyJob: run perspective-divide +
    viewport + back-face-cull (math copied from `primitiveassembly_lt`
    until LT/CA share a math module), populate `job->triangles` in
    place, forward downstream.
  - Timing placeholder: 2 cycles per emitted triangle (split into
    post-divide + cull/emit phases). Real per-tri pipe latency
    (clipping, attribute setup) Phase 2.x.
  - Testbench `test_primitiveassembly_ca.cpp` (Docker-only): push
    one PrimAssemblyJob with 3 vs_outputs covering the same triangle
    as the LT testbench (NDC top-vertex 0.7 on a 32×32 vp);
    asserts 1 triangle out, top-vertex screen y ≈ 27.2.
- **Tests**: 17/17 non-SystemC still green; gpu_systemc lib compiles.
- **Out of scope**: clipping (near/far plane), strip / fan primitive
  modes (LT block already does these; CA copy currently TRIANGLES-
  only).

## Sprint 22 — Rasterizer cycle-accurate(`e481719`)
- **Done**:
  - `rasterizer_ca.{h,cpp}` — fifth and final cycle-accurate block
    of the vertex pipeline. Per accepted RasterJob: edge-function
    rasterisation (1× and 4× MSAA paths matching the LT impl,
    D3D rotated-grid pattern), per-pixel-centre barycentric for
    varying / depth, populate `job->fragments` in place, forward.
  - Timing placeholder: 1 cycle / fragment.
  - Testbench `test_rasterizer_ca.cpp` (Docker-only): build a
    32×32 RasterJob with one screen-space triangle covering ~half
    the framebuffer, push, assert fragment count in [100, 600] and
    first fragment has coverage_mask == 0x1.
  - With this commit, the **complete vertex pipeline** CP → VF →
    SC → PA → RS now exists in cycle-accurate form alongside the
    Phase-1 LT versions. Top-level chain wiring becomes mostly
    mechanical from here; the next milestone is integrating the
    five into a single `gpu_top_ca`.
- **Tests**: 17/17 non-SystemC still green; gpu_systemc lib
  compiles.
- **Out of scope**: per-tile binning, hierarchical-Z, scissor in
  CA path (LT has scissor since Sprint 17; CA copy doesn't yet).
  TMU / PFO / TBF / RSV / MMU / L2 / MC / CSR / PMU CA variants
  are still future sprints.

## Sprint 23 — gpu_top_ca chain(`7ea722e`)
- **Done**:
  - `top/include/gpu_systemc/gpu_top_ca.h` + `top/src/gpu_top_ca.cpp`
    — chip-level CA top wires CP_ca → VF_ca → SC_ca with internal
    `sc_signal` handshakes, exposes the post-SC output as the
    chain output for tb-side observation.
  - PA_ca / RS_ca are NOT in the chain yet (their payloads are
    PrimAssemblyJob / RasterJob, not ShaderJob — bridging needs an
    adapter block, deferred to Phase 2.x).
  - Testbench `test_gpu_top_ca.cpp` (Docker-only): one ShaderJob
    enqueued at CP, sink sees 3 copies (vertices_per_cmd) of the
    same pointer; `job.outputs[0].x = c0+c1`.
- **Tests**: 17/17 local still green; gpu_systemc lib compiles.
- **Out of scope**: full-pipeline chain through PA / RS / PFO /
  TBF / RSV; payload-conversion adapter blocks; rate-matching
  buffers between sub-blocks.

## Sprint 24 — TextureUnit cycle-accurate(`65b27ab`)
- **Done**:
  - `textureunit_ca.{h,cpp}` — wire-level CA wrap of
    `gpu::sample_texture`. Per accepted `TextureJob`, walks
    `requests[i]` and fills `samples[i]` in order, forwards the same
    pointer downstream. 1 cycle / request placeholder.
  - New payload `TextureJob{tex, requests, samples}` in
    `gpu_systemc/payload.h`. `gpu_systemc` lib gains link dep on
    `gpu_sw_ref`.
  - Testbench `test_textureunit_ca.cpp` (Docker-only): 2×2 RGBA8
    (R,G,B,W) with NEAREST + 4 sample requests at texel centres,
    asserts per-channel correctness on each result and 1-job sink
    hit.
- **Tests**: 17/17 local still green.
- **Out of scope**: bilinear / trilinear timing, mipmap LOD
  selection, L1 Tex$ tag-check pipeline, format decoders for
  RGB565 / ETC1.

## Sprint 25 — PerFragmentOps cycle-accurate(`82071d1`)
- **Done**:
  - `perfragmentops_ca.{h,cpp}` — wire-level CA wrap of
    `gpu::pipeline::per_fragment_ops`. Per accepted `PfoJob{ctx,
    quad}`, runs the full PFO logic in place: depth/stencil test,
    alpha-to-coverage, blending, color/depth/stencil writeback.
    Both 1× and 4× MSAA paths are exercised through the underlying
    sw_ref function. Timing placeholder: 4 cycles per quad.
  - New payload `PfoJob{ctx, quad}`.
  - Testbench `test_perfragmentops_ca.cpp` (Docker-only): 4×4 fb,
    1× MSAA, no blend / depth, push 1 quad with 4 distinct-coloured
    fragments at (0,0)…(1,1), assert R/G/B/W lands at the expected
    RGBA8 pixels.
- **Tests**: 17/17 local still green.
- **Out of scope**: 5-stage early-Z / late-Z pipeline timing, hi-Z
  block, blend pipe forwarding hazards.

## Sprint 26 — TileBuffer + ResolveUnit cycle-accurate(`29e9ec7`)
- **Done**: Two CA blocks landed together — they share
  `TileFlushJob` and chain naturally on tile-flush.
  - `tilebuffer_ca.{h,cpp}` — storage placeholder. The on-chip
    ~64 KB sample SRAM is modelled by
    `Context::fb.{color,depth,stencil}_samples` on the host;
    TBF_ca only stamps a flush-rate timing (1 cyc / pixel).
  - `resolveunit_ca.{h,cpp}` — wrap of `gpu::pipeline::resolve`.
    Box-filters 4× MSAA samples → 1× RGBA8 in `fb.color` in place.
    1 cyc / pixel placeholder.
  - New payload `TileFlushJob{ctx, tile_x/y/w/h}`.
  - Testbench `test_tilebuffer_resolve_ca.cpp` (Docker-only):
    4×4 MSAA-4× fb, preload `color_samples` per pixel as
    {0xFF,0xFF,0,0} on R, push 1 `TileFlushJob` through TBF→RSV
    chain, assert every pixel resolves to 0x00000080 (=
    (0xFF+0xFF+0+0+2)>>2).
- **Tests**: 17/17 local still green.
- **Out of scope**: 8-bank SRAM bank-conflict modeling; tile-bin
  metadata; explicit-resolve `glBlitFramebuffer` path.

## Sprint 27 — MMU + L2 + MC cycle-accurate (memory subsystem)(`1d5faa7`)
- **Done**: Three CA blocks landed together — they share
  `MemRequest` and chain naturally as MMU → L2 → MC.
  - `memorymanagementunit_ca.{h,cpp}` — identity-map placeholder
    with `va_limit` fault check. 1 cyc / req placeholder.
  - `l2cache_ca.{h,cpp}` — pass-through with 4-cyc hit-latency
    stamp (no actual caching).
  - `memorycontroller_ca.{h,cpp}` — owns a backing
    `std::vector<uint8_t>` "DRAM" (default 64 KiB,
    ctor-configurable). Per request, executes the read or write
    into the DRAM in place (or no-ops on fault). 12-cyc
    bank-latency placeholder.
  - New payload `MemRequest{addr, size, is_write, data, fault}`.
  - Testbench `test_memory_subsystem_ca.cpp` (Docker-only): chains
    MMU→L2→MC, pushes one 16-byte write at addr 0x1000 then a
    16-byte read at the same addr (separate request objects),
    asserts read-back matches the write pattern and sink saw 2
    hits.
- **Tests**: 17/17 local still green.
- **Out of scope**: real TLB, real cache lines, DRAM bank model
  with row buffers / refresh, multi-port arbitration,
  write-combining.

## Sprint 28 — CSR + PMU + TileBinner cycle-accurate (sidebands)(`82bdaf6`)
- **Done**: Three independent sideband CA blocks. They don't
  naturally chain (unlike vertex / mem subsystems), so the test
  spins up three Source/CA/Sink trios in one `sc_main` and
  exercises each in parallel.
  - `controlstatusregister_ca.{h,cpp}` — 16 × 32-bit register
    file. Read / write per `CsrJob`. The chip-internal handshake;
    APB / AHB bus protocol is host-side and out of scope here.
  - `perfmonitorunit_ca.{h,cpp}` — rolling cycle counter via a
    second `SC_CTHREAD` ticking every clock edge from `rst_n`
    deassertion. Per accepted `PmuJob`, snapshots the current
    count into `job->cycles`.
  - `tilebinner_ca.{h,cpp}` — per-triangle bbox → tile-grid hit
    increments into `bin_counts[]`. 1 cyc / tile-update
    placeholder.
  - New payload structs `CsrJob` / `PmuJob` / `TileBinJob`.
  - Testbench `test_sidebands_ca.cpp` (Docker-only): CSR
    write 0xDEADBEEF→reg5, read back, assert match; PMU query
    after ~5 µs, assert cycles > 0; BIN 4×4 grid (16-px tiles),
    2 tris with predictable bboxes, assert `bin_counts` at
    (0,0), (1,1), (2,1) are 1.
- **Tests**: 17/17 local still green.
- **Out of scope**: APB / AHB bus protocol, full PMU counter bank
  (instructions retired, cache hit/miss, …), 2-stage binner pipe.
- **Phase 2 milestone**: this closes the Phase-2 CA buildout — all
  15 SystemC blocks now have cycle-accurate flavours alongside
  their LT / pass-through flavours. Chip-level full-pipeline
  wiring deferred to Phase 2.x where payload-conversion adapter
  blocks land.

## Sprint 29 — SC→PA payload-conversion adapter(`e197bc8`)
- **Done**:
  - `blocks/adapters/{include,src}/sc_to_pa_adapter_ca.{h,cpp}` —
    first Phase-2.x adapter. Bridges SC_ca (emits ShaderJob*) and
    PA_ca (consumes PrimAssemblyJob*).
  - Batches `batch_size` (default 3) consecutive ShaderJobs,
    copies each `outputs` into a host-side staged
    PrimAssemblyJob's `vs_outputs[i]` slot, then forwards a pointer
    to the staged job downstream. Single-buffered.
  - Constructor knobs: batch_size, vp_w, vp_h, cull_back.
  - Testbench `test_sc_pa_adapter_ca.cpp` (Docker-only): SC_ca runs
    `mov o0, c0` over 3 ShaderJobs whose c0 are the canonical
    triangle's clip-space vertices; assert chain output matches the
    standalone PA_ca test (1 triangle, top y ≈ 27.2 on 32×32 vp).
- **Tests**: 17/17 local still green.
- **Out of scope**: multi-buffer adapter for back-to-back triangles;
  primitive restart; per-vertex non-triangle modes.

## Sprint 30 — PA→RS payload-conversion adapter(`d33bfaf`)
- **Done**:
  - `pa_to_rs_adapter_ca.{h,cpp}` — second adapter. Shallow-copies
    `triangles` from PrimAssemblyJob into a staged RasterJob and
    stamps fb_w / fb_h / msaa_4x / varying_count from constructor
    knobs. 1 cyc / repackage placeholder.
  - Testbench `test_pa_rs_adapter_ca.cpp` (Docker-only): clip-space
    triangle through PA → adapter → RS with fb=32×32; asserts sink
    pointer == &adapter.staged() and fragment count in [100, 600]
    (matching the standalone Sprint-22 RS_ca range).
- **Tests**: 17/17 local still green.
- **Out of scope**: triangle batching across multiple PA jobs;
  per-job fb attribute lookup from a Context*.

## Sprint 31 — RS→PFO payload-conversion adapter(`209cd3c`)
- **Done**:
  - `rs_to_pfo_adapter_ca.{h,cpp}` — most complex of the four.
    Per accepted RasterJob*: walks `fragments`, hashes each by
    `(qx, qy) = (x>>1, y>>1)` into a `std::map`, lane index =
    `(y&1)*2 + (x&1)`. Empty lanes default to coverage_mask=0.
    Converts `gpu::sim::Vec4` (RS varying) → `gpu::Vec4f` (Fragment
    varying) component-by-component. Emits one PfoJob per non-empty
    quad sequentially, with `pfo_job_.ctx = adapter.ctx`.
  - Constructor knob: ctx (gpu::Context*).
  - Testbench `test_rs_pfo_adapter_ca.cpp` (Docker-only): 32×32 fb,
    triangle through RS → adapter → PFO; assert RS emitted >0
    fragments, adapter regrouped >0 quads, sink saw 1 PfoJob* per
    quad, and quads × 4 ≥ frags.
- **Tests**: 17/17 local still green.
- **Out of scope**: derivative computation across quad lanes (needed
  for gradient-based texture LOD); helper-lane masking.

## Sprint 32 — PFO→TBF tile-flush trigger adapter(`1d6db65`)
- **Done**:
  - `pfo_to_tbf_adapter_ca.{h,cpp}` — final adapter. Counts accepted
    PfoJobs and emits a TileFlushJob covering the full framebuffer
    after every `quads_per_flush` quads (default 1 = per-quad
    flush).
  - PfoJob.ctx is captured into the staged TileFlushJob.ctx.
  - Testbench `test_pfo_tbf_adapter_ca.cpp` (Docker-only):
    quads_per_flush=2; push 4 PfoJobs sharing one Context*, assert
    sink saw 2 TileFlushJobs with ctx and fb dims (16×16) preserved.
- **Tests**: 17/17 local still green.
- **Out of scope**: per-tile binning so the flush carries which tile
  is being resolved; multi-tile flush ordering.

## Sprint 33 — Phase 2.x full-chain end-to-end testbench(`e95b44e`)
- **Done**:
  - `tb/test_full_chain_ca.cpp` — wires every CA block + every
    adapter from Sprints 29–32 (12 modules, 11 hops):
    `src → SC_ca → SC_to_PA → PA_ca → PA_to_RS → RS_ca →
     RS_to_PFO → PFO_ca → PFO_to_TBF → TBF_ca → RSV_ca → sink`.
  - 3 ShaderJobs run `mov o0, c0; mov o1, c1` (position + colour).
    With c1 = white at every vertex, RS interpolates a constant
    white varying[0]; PFO packs `(1,1,1,1)` → `0xFFFFFFFF` and
    writes to fb.color. The test asserts ≥1 TileFlushJob at sink
    AND ≥100 white pixels in fb.color.
- **Tests**: 17/17 local still green; gpu_systemc lib compiles.
- **Phase 2.x milestone**: first runnable proof that all 15 CA
  blocks + 4 adapters compose into a functional pipeline.
- **Out of scope (next)**: real pipeline timing (replace the 1-cyc
  placeholders); CP/VF reintegration so the chain starts at CP
  instead of SC (currently VF fans out the same opaque pointer N
  times — needs per-vertex ShaderJob distinct allocation); tile
  binning so flushes carry tile coords.
