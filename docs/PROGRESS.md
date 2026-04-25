---
doc: Progress Log
purpose: Human-readable index of what shipped per commit, mapped to Master Plan milestones.
last_updated: 2026-04-25
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

---

## Status snapshot

- **Master Plan phase**: Phase 1 (Three-Track Parallel)
- **Tests passing**: 9/9 (CTest, local macOS / GCC-15)
  - `compiler.{asm_roundtrip, sim_basic, glsl_compile, sim_warp}`
  - `sw_ref.{basic, fp, isa_triangle, msaa, texture}`
  - `systemc.tlm_hello` skipped on macOS (libc++/libstdc++ ABI), runs in Docker
- **ISA**: v1.0 frozen; 2 known gaps tracked (MEM dst/src class bits)
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
  per-lane break with reconvergence is Phase 1.x.
- **Next**: TBD — see candidates list.

---

## Open follow-ups (not yet sprinted)

| Topic | Notes |
|---|---|
| **ISA v1.1** | Steal 2 bits from MEM `imm` for `dst_class` + `src_class`; remove the `mov rN, vN; tex; mov oN, rN` workaround. Also fix per-lane `break` semantics. |
| **PFO depth/stencil/blend** | Currently PFO writes color directly. ES 2.0 needs depth test, stencil ops, blend equations. |
| **More TLM blocks** | Only CP + SC exist in TLM. Need VF / PA / RS / TMU / PFO / TBF / RSV / MMU / L2 / MC / CSR / PMU. |
| **GLSL frontend expansion** | Sprint 2 covers ref_shader_1 patterns. ref_shader_2 (Phong) needs `dot`/`normalize`/`pow`/`max`. ref_shader_3 needs `if`/`for`/`discard`. |
| **Conformance harness** | dEQP / ES 2.0 CTS subset wired to CI. |
| **glslang integration** | Replace hand-written GLSL parser with glslang → SPIR-V → IR. Architecture already designed for this swap. |
| **Per-block microarch** | 15 microarch docs are draft v0.1. Will iterate as TLM blocks land. |
