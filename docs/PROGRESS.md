---
doc: Progress Log
purpose: Human-readable index of what shipped per commit, mapped to Master Plan milestones.
last_updated: 2026-04-25 (post-Sprint 12 refactor)
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

---

## Status snapshot

- **Master Plan phase**: Phase 1 (Three-Track Parallel)
- **Tests passing**: 14/14 (CTest, local macOS / GCC-15)
  - `compiler.{asm_roundtrip, sim_basic, glsl_compile, sim_warp, warp_break, glsl_ext}`
  - `sw_ref.{basic, fp, isa_triangle, msaa, texture, pfo}`
  - `conformance.{triangle_white, triangle_msaa}`
  - Skipped (Docker-only): `systemc.tlm_hello`, `compiler.glsl_to_spv`
- **ISA**: v1.1 frozen (MEM class bits + per-lane break formalised)
- **Optional gates**: `-DGPU_BUILD_SYSTEMC=ON` (TLM CP→VF→SC + PA),
  `-DGPU_BUILD_GLSLANG=ON` (FetchContent glslang, GLSL → SPIR-V CLI)
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
| **PFO depth/stencil/blend** | Currently PFO writes color directly. ES 2.0 needs depth test, stencil ops, blend equations. |
| **More TLM blocks** | Only CP + SC exist in TLM. Need VF / PA / RS / TMU / PFO / TBF / RSV / MMU / L2 / MC / CSR / PMU. |
| **GLSL frontend expansion** | Sprint 2 covers ref_shader_1 patterns. ref_shader_2 (Phong) needs `dot`/`normalize`/`pow`/`max`. ref_shader_3 needs `if`/`for`/`discard`. |
| **Conformance harness** | dEQP / ES 2.0 CTS subset wired to CI. |
| **glslang integration** | Replace hand-written GLSL parser with glslang → SPIR-V → IR. Architecture already designed for this swap. |
| **Per-block microarch** | 15 microarch docs are draft v0.1. Will iterate as TLM blocks land. |

**Closed by Sprint 7**:
- ~~ISA v1.1: MEM `dst_class` + `src_class`; per-lane `break` semantics~~

**Closed by Sprints 8–12**:
- ~~PFO depth/stencil/blend~~ (depth + blend done Sprint 8; stencil still TODO)
- ~~More TLM blocks: VF + PA~~ (RS/TMU/PFO/TBF/RSV/MMU/L2/MC/CSR/PMU still TODO)
- ~~GLSL frontend ext: dot/normalize/max/min/clamp/pow + if/else + locals~~
  (vector ctors, for/while, reflect/length/abs still TODO)
- ~~Conformance harness~~ (scene format simple, scene-level shader binding TODO)
- ~~glslang integration~~ (SPIR-V emit done; SPIR-V → IR lowering still TODO)

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
  - `VertexFetch` (TLM target+initiator) and `PrimitiveAssembly` (TLM
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
  build paths). Class names (`CommandProcessor`, etc.) were already in
  full form; only paths changed.
- **Out of scope**: `docs/microarch/<abbr>.md` filenames left as-is
  (those are spec stubs, not code; rename is cheap if asked later).
