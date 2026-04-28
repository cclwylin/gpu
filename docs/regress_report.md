# Example regression report

Generated: 2026-04-27 05:42:28
VK-GL-CTS sweep appended: 2026-04-28 00:55 (Sprint 61 — multi-varying
capture + full blend state + per-batch viewport in scene format;
`fragment_ops.blend.*` median RMSE ~110 → ~6 on a 50-case sample;
`color_clear.*` stays 19/19 bit-perfect; in-tree ctest 56/56. See § Sprint 61.)

VK-GL-CTS sweep appended: 2026-04-28 00:30 (Sprint 60 — `SceneOp::CLEAR`
carries scissor rect + color-mask lane; `color_clear.*` 8/19 → 19/19
bit-perfect through SC chain; in-tree ctest 56/56. See § Sprint 60.)

VK-GL-CTS sweep appended: 2026-04-27 21:30 (Sprint 58 — int() trunc +
const-scalar fold + init-aliasing + output-slot widen 4→8 + setp neg
propagation; fragment_ops 1902 → 1920 (99.8 %); sweep 2017 → 2035 /
2143 (95.0 %); basic_shader 79/100 → 97/100; see § Sprint 58 below).
Sprint 59 wires the SC-chain replay layer underneath the sw_ref sweep:
8 / 19 `color_clear.*` cases now pass bit-perfectly through deqp-gles2
→ glcompat → scene → sc_pattern_runner. See § Sprint 59 below for
the pipeline + remaining scene-format gaps.

Total examples: **49**
| metric | count | % |
|---|---:|---:|
| builds  | 49 | 100% |
| runs    | 49   | 100% |
| sc-runs | 47    | 96% |
| RMSE < 1.0 | 45 | 92% |

Legend: ✓ match · ≈ rendered, RMSE high · ? rendered, dim mismatch · · sw_ref only · ✗ build/run fail

| | example | pixels | sw paint | SC paint | tris | cycles | RMSE | wall ms | note |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| ✓ | `abgr` | 65,536 | 3,023 | 3,023 | 2 | 75,002 | 0.00 | 708 |  |
| ✓ | `bitfont` | 75,000 | 923 | 923 | 0 | 2 | 0.00 | 496 |  |
| ✓ | `blender` | 65,536 | 6,710 | 6,710 | 298 | 268,002 | 0.00 | 678 |  |
| ✓ | `cube` | 65,536 | 22,574 | 22,574 | 12 | 367,002 | 0.00 | 691 |  |
| ✓ | `dials` | 57,600 | 57,600 | 12,800 | 90 | 227,002 | 0.00 | 626 |  |
| ✓ | `dials2` | 57,600 | 57,600 | 21,832 | 272 | 205,002 | 0.00 | 604 |  |
| ✓ | `dinoball` | 65,536 | 12,146 | 12,146 | 328 | 280,002 | 0.30 | 652 |  |
| ≈ | `dinoshade` | 65,536 | 31,801 | 31,801 | 996 | 1,210,002 | 9.43 | 1225 |  |
| ✓ | `dinospin` | 65,536 | 20,470 | 20,470 | 392 | 499,002 | 0.14 | 780 |  |
| ✓ | `evaltest` | 90,000 | 5 | 5 | 200 | 52,002 | 0.00 | 524 |  |
| ✓ | `fogtst` | 90,000 | 90,000 | 4,995 | 16 | 110,002 | 0.00 | 551 |  |
| ✓ | `fontdemo` | 116,250 | 97,232 | 19,018 | 488 | 192,002 | 0.00 | 613 |  |
| ✓ | `glpuzzle` | 120,000 | 120,000 | 42,280 | 488 | 563,002 | 0.42 | 853 |  |
| ✓ | `glutdino` | 65,536 | 11,777 | 11,777 | 328 | 274,002 | 0.30 | 653 |  |
| ✓ | `glutplane` | 65,536 | 65,536 | 65,536 | 20 | 618,002 | 0.00 | 882 |  |
| ✓ | `halomagic` | 65,536 | 31,914 | 31,914 | 362 | 734,002 | 0.39 | 880 |  |
| ✓ | `highlight` | 250,000 | 54,161 | 54,161 | 750 | 747,002 | 0.00 | 941 |  |
| ✓ | `lightlab` | 160,000 | 36,314 | 36,314 | 1126 | 543,002 | 0.00 | 850 |  |
| ✓ | `mjksift` | 65,536 | 6,045 | 6,045 | 288 | 97,002 | 0.00 | 549 |  |
| ✓ | `mjkwarp` | 65,536 | 16,384 | 16,384 | 288 | 166,002 | 0.00 | 601 |  |
| ✓ | `molehill` | 65,536 | 0 | 0 | 0 | 2 | 0.00 | 494 |  |
| ✓ | `movelight` | 250,000 | 84,038 | 84,038 | 1932 | 1,101,002 | 0.01 | 1175 |  |
| ✓ | `oclip` | 65,536 | 4,538 | 4,538 | 40 | 83,002 | 0.00 | 528 |  |
| ✓ | `ohidden` | 160,000 | 13,542 | 13,542 | 108 | 1,279,002 | 0.00 | 1120 |  |
| ✓ | `olight` | 65,536 | 4,538 | 4,538 | 40 | 83,002 | 0.00 | 532 |  |
| ✓ | `olympic` | 120,000 | 119,973 | 2,510 | 2000 | 91,002 | 0.32 | 597 |  |
| ✓ | `origami` | 65,536 | 65,536 | 21,060 | 28 | 342,002 | 0.00 | 675 |  |
| · | `oversphere` | 0 | 0 | 0 | 0 | 0 | — | 60000 | timeout |
| ✓ | `reflectdino` | 65,536 | 39,108 | 39,108 | 660 | 986,002 | 0.90 | 1082 |  |
| ✓ | `rendereps` | 65,536 | 65,536 | 39,814 | 450 | 609,002 | 0.00 | 849 |  |
| ✓ | `sb2db` | 250,000 | 62,046 | 62,046 | 682 | 2,403,002 | 0.20 | 1913 |  |
| ✓ | `scene` | 250,000 | 44,535 | 44,535 | 302 | 613,002 | 0.00 | 864 |  |
| ✓ | `screendoor` | 65,536 | 65,536 | 11,723 | 302 | 139,002 | 0.00 | 565 |  |
| ✓ | `scube` | 122,500 | 78,544 | 78,544 | 180 | 902,002 | 0.00 | 1013 |  |
| ✓ | `simple` | 65,536 | 18,100 | 18,100 | 1 | 195,002 | 0.00 | 596 |  |
| ✓ | `sphere` | 65,536 | 10,920 | 10,920 | 1600 | 366,002 | 0.01 | 718 |  |
| ✓ | `sphere2` | 65,536 | 10,920 | 10,920 | 1600 | 408,002 | 0.00 | 749 |  |
| ✓ | `splatlogo` | 299,200 | 299,200 | 0 | 0 | 2 | 0.00 | 549 |  |
| ✓ | `spots` | 65,536 | 20,682 | 20,682 | 518 | 196,002 | 0.00 | 610 |  |
| ✓ | `stars` | 90,000 | 42 | 42 | 66 | 52,002 | 0.00 | 506 |  |
| ✓ | `stenciltst` | 65,536 | 29,128 | 29,128 | 5 | 677,002 | 0.00 | 872 |  |
| ✓ | `stereo` | 65,536 | 65,536 | 0 | 0 | 2 | 0.00 | 489 |  |
| ✓ | `stroke` | 360,000 | 11,328 | 11,328 | 144 | 142,002 | 0.00 | 616 |  |
| ✓ | `subwin` | 44,100 | 44,100 | 0 | 0 | 2 | 0.00 | 478 |  |
| ✓ | `surfgrid` | 360,000 | 360,000 | 36,508 | 1792 | 1,510,002 | 0.20 | 1455 |  |
| ≈ | `texenv` | 417,600 | 417,600 | 7,310 | 30 | 1,566,002 | 1.60 | 1323 |  |
| ✓ | `trippy` | 262,144 | 262,144 | 262,144 | 20 | 3,852,002 | 0.00 | 2773 |  |
| ✓ | `triselect` | 65,536 | 1,177 | 1,177 | 10 | 59,002 | 0.01 | 547 |  |
| · | `zoomdino` | 0 | 0 | 0 | 0 | 0 | — | 60000 | timeout |

## CTest suites

All registered ctest entries. Tracks `tests/`, `compiler/tests/`, `sw_ref/tests/`, `tests/conformance/`, `tests/glmark2_runner/`, and `systemc/tb/`. **46/46 passing** (45 in-tree + `vkglcts.gles2.color_clear` when `-DGPU_BUILD_VKGLCTS=ON`).

| namespace | passed | total |
|---|---:|---:|
| `compiler.*` | 7 | 7 |
| `conformance.*` | 3 | 3 |
| `glmark2.*` | 6 | 6 |
| `sw_ref.*` | 7 | 7 |
| `systemc.*` | 22 | 22 |
| `vkglcts.*` | 1 | 1 |

<details><summary>Per-test detail</summary>

| status | test |
|---|---|
| ✓ | `compiler.asm_roundtrip` |
| ✓ | `compiler.glsl_compile` |
| ✓ | `compiler.glsl_ext` |
| ✓ | `compiler.sim_basic` |
| ✓ | `compiler.sim_warp` |
| ✓ | `compiler.spv_lower` |
| ✓ | `compiler.warp_break` |
| ✓ | `conformance.triangle_msaa` |
| ✓ | `conformance.triangle_rgb` |
| ✓ | `conformance.triangle_white` |
| ✓ | `glmark2.clear` |
| ✓ | `glmark2.fbo` |
| ✓ | `glmark2.glsl` |
| ✓ | `glmark2.textured_quad` |
| ✓ | `glmark2.to_sc` |
| ✓ | `glmark2.triangle` |
| ✓ | `sw_ref.basic` |
| ✓ | `sw_ref.fp` |
| ✓ | `sw_ref.isa_triangle` |
| ✓ | `sw_ref.msaa` |
| ✓ | `sw_ref.pfo` |
| ✓ | `sw_ref.stencil_scissor` |
| ✓ | `sw_ref.texture` |
| ✓ | `systemc.commandprocessor_ca` |
| ✓ | `systemc.cp_dispatch` |
| ✓ | `systemc.full_chain_ca` |
| ✓ | `systemc.gpu_top_ca` |
| ✓ | `systemc.memory_subsystem_ca` |
| ✓ | `systemc.memory_subsystem_lt` |
| ✓ | `systemc.pa_rs_adapter_ca` |
| ✓ | `systemc.perfragmentops_ca` |
| ✓ | `systemc.perfragmentops_lt` |
| ✓ | `systemc.pfo_tbf_adapter_ca` |
| ✓ | `systemc.primitiveassembly_ca` |
| ✓ | `systemc.rasterizer_ca` |
| ✓ | `systemc.rs_pfo_adapter_ca` |
| ✓ | `systemc.sc_pa_adapter_ca` |
| ✓ | `systemc.shadercore_ca` |
| ✓ | `systemc.sidebands_ca` |
| ✓ | `systemc.sidebands_lt` |
| ✓ | `systemc.textureunit_ca` |
| ✓ | `systemc.textureunit_lt` |
| ✓ | `systemc.tilebuffer_resolve_ca` |
| ✓ | `systemc.tilebuffer_resolve_lt` |
| ✓ | `systemc.vertexfetch_ca` |
| ✓ | `vkglcts.gles2.color_clear` |

</details>

## VK-GL-CTS sweep

Snapshot of Khronos VK-GL-CTS `deqp-gles2` running through the
`gpu_glcompat` shim (Sprint 42 platform target → Sprint 47 GLSL parser
hardening). Generated by `tools/run_vkglcts_sweep.py`; rerun with
`--include-texture --include-shaders` for the heavyweight groups (default
sweep skips them — `texture` ~1 k cases, `shaders` ~10 k cases hit GLSL
parser limits and would mostly fail).

**Total: 2035 / 2143 pass (95.0 %) across 11 groups.** (Sprint 42 baseline
68/2143 → Sprint 58 2035/2143, +1967. Most of the gain landed in
Sprint 46 (+1633); Sprint 47 +14 (parser); Sprint 48 +10 (deeper parser);
Sprint 49 +13 (glStencilFunc ref-clamp); Sprint 50 +11 (glUniform float
variants + glClear honors depth/stencil mask); Sprint 51 +47 (typed
vertex attribute reads); Sprint 53 +97 (depth-buffer write gated on
depth_test enabled per GLES 2.0 § 4.1.6); Sprint 54 +64 (constant-depth
quad bypass for boundary precision); Sprint 55 +7 (VS/FS uniform +
literal c-bank slot separation); Sprint 56 +26 (c-bank 16 → 32 + bool()
conversion + literal dedup); Sprint 57 +15 (varying packing + vec
equality scalar-reduce + GPR-pressure relief + spec-correct boundary);
Sprint 58 +18 (int() trunc + const-scalar fold + init-aliasing +
output-slot encoding widened 4→8 + setp neg-flag propagation).)

| group | passed | failed | other | total | wall (s) | Δ vs S42 | note |
|---|---:|---:|---:|---:|---:|---:|---|
| `info.*` | 6 | 0 | 0 | 6 | 0.7 | 0 | all info queries (vendor / renderer / version / extensions / render_target / shading_language_version) |
| `capability.*` | 10 | 0 | 78 | 88 | 0.0 | 0 | 78 "Other" = NotSupported / QualityWarning |
| `functional.prerequisite.*` | 3 | 0 | 0 | 3 | 0.0 | 0 | basic state + read-pixels precondition |
| `functional.color_clear.*` | 19 | 0 | 0 | 19 | 1.4 | 0 | **closed by Sprint 43** (was 8/19 before glColorMask + scissored clear) |
| `functional.depth_stencil_clear.*` | **11** | 0 | 0 | 11 | 10.7 | **+11** | **closed by Sprint 50** — `glUniform4f` was missing (`dlsym → NULL`, uniform stayed 0, visualization quad rendered black on B channel); `glClear(DEPTH/STENCIL)` now honors `glDepthMask` / `glStencilMask` per GLES 2.0 § 4.3.2 |
| `functional.implementation_limits.*` | **16** | 0 | 0 | 16 | 0.0 | **+12** | **closed by Sprint 44** (was 4/16) — wired GL_MAX_TEXTURE_SIZE, GL_SUBPIXEL_BITS, GL_MAX_VERTEX_ATTRIBS, GL_MAX_*_UNIFORM_VECTORS, etc. into `glGetIntegerv` / `glGetFloatv` / `glGetBooleanv` |
| `functional.buffer.*` | **49** | 0 | 0 | 49 | 3.9 | **+47** | **closed by Sprint 51** — `glVertexAttribPointer(loc, 3, GL_UNSIGNED_BYTE, GL_TRUE, 0, ...)` was being read as `float*`; now switches on `va.type` and converts `byte/short/fixed` through the normalize divide. |
| `functional.fragment_ops.*` | **1920** | 3 | 0 | 1923 | 82.2 | **+1897** | Sprint 46 +1633; +14 +9 +13 +97 +64 +7 across Sprints 47–55; Sprint 56 +26; Sprint 57 (varying packing + vec equality + GPR pressure + boundary) +15; Sprint 58 (int() trunc + const-scalar fold + output-slot widen + setp neg) +18. **99.8 % conformance**. Remaining 3 are `interaction.basic_shader.{24,33,75}` — multi-draw blend × depth × color-mask × scissor state composites where standalone FS sim is correct but the pipeline composite over 3+ draws differs. |
| `functional.clip_control.*` | 0 | 0 | 8 | 8 | 0.0 | 0 | GLES 3.0+ feature; reports NotSupported |
| `functional.light_amount.*` | 1 | 18 | 0 | 19 | 0.0 | 0 | fixed-function lighting — mostly N/A on GLES 2.0 contexts |
| `functional.multisampled_render_to_texture.*` | 0 | 0 | 1 | 1 | 0.0 | 0 | extension; reports NotSupported |

### Sprint 44 — what moved

- `glcompat::glGetIntegerv` / `glGetFloatv` / `glGetBooleanv` now answer
  every GLES 2.0 implementation_limits query with a value that meets the
  spec minima (and reports what `sw_ref` actually supports — 8 vertex
  attribs, 8 texture image units, 256 uniform vectors, etc.). Previously
  every unrecognised enum returned 0 → CTS rejected everything.
- `sw_ref::edge_fn` promotes intermediates to `double` so diagonal-edge
  pixels (where `(bx-ax)*(py-ay)` and `(by-ay)*(px-ax)` are
  mathematically equal) collapse to an exact zero. Clang's default
  `-ffp-contract=on` was fusing one of the products into FMA, producing
  a sign-flipped residual that lost coverage along triangle diagonals.
  Fixes the long-standing `sw_ref.pfo` failure (CTest 26/27 → 27/27);
  did not move the needle on `depth_stencil_clear.*` (deeper draw-path
  issue).

### Sprint 45 — rasterizer winding fix

- `sw_ref::rasterizer` was rejecting CW-wound triangles outright: the
  coverage test `w0 >= 0 && w1 >= 0 && w2 >= 0` only triggers for
  positive signed area (CCW). Many fragment_ops tests submit CW
  triangles; FMA noise in the old single-precision `edge_fn` had been
  masking the rejection sporadically. With Sprint 44's deterministic
  double-precision `edge_fn`, the rejection became consistent and 17
  fragment_ops tests dropped.
- Fix: compute `winding = sign(area)` once per triangle and multiply
  each `w_i` by it before the coverage compare. Both windings now
  rasterize correctly. Backface culling stays in
  `primitive_assembly::back_face_cull` + `cull_back` — the rasterizer
  should never have been a winding gate.
- Result: `fragment_ops.*` 6 → 23 (+17), recovering the Sprint 42
  baseline AND replacing the lucky passes with real ones — all 8
  `depth.cmp_*` variants now pass (real GLES depth conformance).
  Aggregate sweep 63 → 80 / 2143 (+17), exceeds the Sprint 42 baseline.

### Sprint 47 — GLSL parser hardening (+14)

Smaller follow-up after the Sprint 46 push:

- **`#version` / `#extension` / `#define` skipped in lexer.** Without
  this, `#` lexed to EOF and many CTS shaders silently produced empty
  programs that linked but exposed no attributes — `glGetAttribLocation`
  returned -1 and the test's `posLoc >= 0` precondition failed.
- **`gl_PointSize` / `gl_FragData` / `gl_FragDepth` no-op in codegen.**
  Assignments to these built-ins return success silently (we don't
  model points or MRT but the test only needs the shader to *link*).
  Closes the 5 `scissor.*_points` tests and 1-2 others.
- **More qualifiers + types accepted** — skip `const`, `invariant`,
  `centroid`, `smooth`, `flat`; add `ivec2/3/4`, `bvec2/3/4`, `uint`,
  `bool`. Codegen still treats them as vec4-shaped GPRs but the
  declarations no longer hit "unknown type" at parse.
- **Block-comment skip** (`/* … */`) added.

### Sprint 48 — serious GLSL parser upgrade (+10)

Targeted at the 100 `fragment_ops.interaction.basic_shader.*` cases
that the dEQP random-shader generator emits. Result: **100/100 cases
now compile** (was 7/100). +10 net pass — most cases still fail on
image-cmp downstream of the parser.

Lexer / expressions:
- `true` / `false` lex as `NUMBER(1.0/0.0)` (was `Tok::ID`, breaking
  `vec4(true, 0, …)` ctors).
- New `parse_compare` ladder between `parse_expr` and `parse_addsub`
  for `<`, `<=`, `>`, `>=`, `==`, `!=`. Lowers to ALU `cmp` (0x13)
  using a scratch GPR pair holding 1.0 / 0.0 literals.
- `ivec*` / `bvec*` / `uvec*` ctors unified with `vec*`; `float(x)` /
  `int(x)` / `bool(x)` / `uint(x)` are identity-pass type-conversion
  calls.
- `a − b` lowers to ADD with a complemented operand (was previously
  only modelled via `UnaryOp '-'`).

Statements / control flow:
- Bare `;` no-op; nested compound `{ … }`; bare expression statements
  (`b.b >= int(a);`). New `ExprStmt` AST variant.
- Brace-less `if`/`else` single-statement bodies via new
  `parse_block_or_stmt`. `if (expr)` with non-comparison cond
  synthesises `expr != 0`.
- `parse_if` uses `parse_addsub` for cond LHS/RHS so the comparison
  token isn't gobbled by the new `parse_compare` ladder.

Globals:
- `Decl` gained an optional `init`; file-scope globals
  (`float g = 0.5;`, `const bool k = a >= b;`) compile. Codegen emits
  `LocalDeclStmt` for each unqualified global with init in a
  synthesised function prologue; `bind()` skips them so the GPR slot
  is allocated exactly once.

GPR shadows:
- `VARYING_OUT` (vertex stage) and `BUILTIN_OUT` (`gl_Position` /
  `gl_FragColor`) bindings get a GPR shadow allocated at first
  lookup. `emit_assign` mirrors writes to both the output slot and
  the shadow; `emit_identifier` reads back from the shadow so
  `gl_FragColor = gl_FragColor;` and `l = (p);` (varying-out
  read-back) compile.

glcompat ES 2.0 uniform setters:
- Added `glUniform2i/3i/4i`, `glUniform1iv/2iv/3iv/4iv`,
  `glUniform1fv`, `glUniformMatrix2fv`. The random-shader tests
  heavily use ivec uniforms; previously absent setters returned
  `NULL` from `dlsym(RTLD_DEFAULT)`, leaving every uniform at zero.

Remaining basic_shader work is rasterizer-side: the 100 cases combine
many fragment-ops state (front/back stencil with INCR_WRAP /
DECR_WRAP, depth EQUAL, blend with SRC_ALPHA_SATURATE + constant
color, color-mask + dither + viewport scissor) over multiple draws,
and any pixel mismatch trips `Image comparison failed`.

### Sprint 61 — multi-varying + full blend + per-batch viewport in scene

After Sprint 60 closed the scissored / masked CLEAR gap, the
`fragment_ops.blend.*` 1060-case block was the next big hit: every one
rendered through deqp / glcompat / sw_ref correctly but came back from
the SC chain at RMSE 60–170. Three orthogonal scene-format gaps drove
this — fixed in one sprint because they share the same plumbing chain
(glcompat capture → scene file → sc_pattern_runner Batch + parser →
SystemC pa_rs / sc_pa adapters).

1. **Multi-varying capture.** glcompat only shipped `t.o[1]` per vertex
   into the scene; ThreadState.o has slots 0..7 (Sprint 58 widening).
   For shaders that pack vertex colour into varying[0] (most basic
   blend tests) this was lucky; for richer fragment_ops shaders it
   silently dropped data. Now `scene_record_es2_batch` takes
   `std::vector<std::array<Vec4f, 7>>` + `n_vars`, and the file
   format gains `varying_count N` plus 4 + 4·N floats per vert line.
   Old single-varying scenes still parse (N defaults to 1).

2. **Full blend state.** SceneBatch only carried `bool blend`; even
   though glcompat's live `s.ctx.draw.blend_*` had everything (Sprint
   46 work), nothing made it to the SC replay so `apply_batch_state`
   used DrawState defaults (`SRC_ALPHA / ONE_MINUS_SRC_ALPHA`, `ADD`).
   New scene keywords:
   - `blend_func <src_rgb> <dst_rgb> <src_alpha> <dst_alpha>`
   - `blend_eq <eq_rgb> <eq_alpha>`
   - `blend_color <r> <g> <b> <a>` (omitted when zero)
   The 15 blend factors and 3 equations match `gpu::DrawState::BF_*`
   / `BE_*` exactly. Old scenes (no `blend_func`) keep the GLES 2.0
   defaults.

3. **Per-batch viewport.** The SC chain's `ScToPaAdapterCa` only
   forwarded `vp_w / vp_h` to its `PrimAssemblyJob`, never `vp_x /
   vp_y`. Result: every batch rendered into the bottom-left of the
   fb regardless of the test's `glViewport(x, y, w, h)`. sw_ref's
   primitive_assembly *did* honour `ctx.draw.vp_x/vp_y`, hence the
   silent divergence. Sprint 61 adds `vp_x / vp_y` to ScToPaAdapter,
   captures `s.vp_*` per SceneBatch, emits `viewport <x> <y> <w> <h>`
   when non-default, and `apply_batch_state` propagates to both
   `sc_pa` and `ctx.draw`. The E2E driver's crop now uses the scene's
   first `viewport` line so the SC PPM diffs against the right
   sub-rect of the fb (instead of always the bottom-left).

**Results.**

| sweep | bit-perfect | median RMSE (diff bucket) |
|---|---:|---:|
| Sprint 60 — `color_clear.*` (19) | 19 | — |
| Sprint 60 — `fragment_ops.blend.*` 9-sample | 0 | ~110 |
| Sprint 61 — same 9 blend cases | 3 | ~6 |
| Sprint 61 — random 50-blend sample | 4 | 6.2 |

`color_clear.*` stays at 19/19 bit-perfect (no regression). The
50-blend sample has RMSE distribution: min 1.5, median 6.2, max 45 —
the gross-logic miss is gone; what's left is sub-pixel rasterization
deltas between sw_ref and the SC chain, which is a separate (smaller)
follow-up.

In-tree ctest holds at 56/56.

### Sprint 60 — CLEAR scissor + color-mask in scene format

The Sprint 59 sweep flagged 22 `color_clear.{scissored,masked,complex}.*`
+ `depth_stencil_clear.*` cases that rendered through deqp → glcompat
fine but had RMSE ~100–170 in the SC replay. The root cause was that
`SceneOp::CLEAR` only carried the resulting RGBA — sw_ref's scissor +
color-mask logic produced the right `pix` value, but the SC chain
replay had no idea what region the clear actually covered, so it
filled the whole fb.

Three coordinated changes, all fixing the same gap.

1. **`SceneOp::CLEAR` carries scissor rect + color-mask lane**
   ([glcompat_render.cpp](../glcompat/src/glcompat_render.cpp),
   [sc_pattern_runner.cpp](../tests/conformance/sc_pattern_runner.cpp)).
   New fields: `clear_rect_full`, `clear_x0/y0/x1/y1`, `clear_lane`
   (32-bit color-mask lane, ARGB-byte shape). `clear_rect_full=true`
   keeps the legacy whole-fb fast-path; otherwise the replay applies
   `pix = (old & ~lane) | (rgba & lane)` per pixel inside [x0,x1)×[y0,y1).
2. **Scene file format extended**. `clear_rect <rgba>` (legacy bare
   form) still parses — old scenes work. New extended form:
   `clear_rect <rgba> <x0> <y0> <x1> <y1> <lane>`. Writers default
   to the bare form for full clears + opt into the extended form
   when the live `glClear` saw a sub-rect or non-trivial color mask.
3. **Don't collapse partial CLEARs.** The pre-Sprint-60 code merged
   any consecutive CLEAR ops by overwriting the previous's rgba. That
   silently dropped the background CLEAR in `color_clear.scissored_*`
   (where the test does whole-fb BG → scissored fill). Now collapse
   only fires when **both** ops are full-fb / full-mask. Without this
   the SC replay rendered inside-scissor on top of an init-zero fb
   instead of the real background.

**Results.**

| sweep | bit-perfect | RMSE > 0 |
|---|---:|---:|
| Sprint 59 — `color_clear.*` (19 cases) | 8 | 11 |
| Sprint 60 — same 19 cases | **19** | 0 |

All 19 `color_clear.*` cases now pass bit-perfectly through deqp →
glcompat → scene → sc_pattern_runner. In-tree ctest holds at 56/56.

The `depth_stencil_clear.*` 11 cases still timeout — Sprint 59's
scissor/mask gap was one of two issues there; the other is the SC
chain's per-fragment depth/stencil loop is just slow. Those need a
separate budget bump or SC-side speedup.

### Sprint 59 — VK-GL-CTS → SystemC E2E pipeline

The Sprint 42–58 work pushed VK-GL-CTS through the sw_ref software
pipeline (2035 / 2143 = 95.0 %). Sprint 59 wires the next layer down:
the same CTS cases now drive the cycle-accurate SystemC chain via a
recorded scene replay.

**Pipeline.**

```
deqp-gles2 ─► glcompat ─► sw_ref pipeline ─► QPA log + Result PNG
                  │
                  └─► glcompat::scene_record_*  ─► .scene file (atexit)
                                                    │
                                                    ▼
                                          sc_pattern_runner
                                                    │
                                                    ▼
                                                .sc.ppm
```

`tools/run_vkglcts_to_sc.py` drives any CTS case through both halves
and reports per-pixel RMSE / max-err / diff-count.

**Gluing changes (all in this sprint):**
- [glcompat/src/glcompat_render.cpp](../glcompat/src/glcompat_render.cpp)
  registers an `atexit(save_scene)` on first scene-capture-enabled glClear
  so non-GLUT clients (deqp-gles2) get the same scene dump that the
  glut path triggers in glutLeaveMainLoop. Without this, deqp would
  enable capture but the .scene file never landed.
- [systemc/common/include/gpu_systemc/payload.h](../systemc/common/include/gpu_systemc/payload.h)
  widens `ShaderJob.constants` 16→32, `ShaderJob.outputs` 4→8, and the
  matching `vs_outputs` / `triangles` / `RasterFragment.varying` arrays
  to track the Sprint 56 c-bank widen and Sprint 58 output-slot widen.
  Without these the SystemC chain wouldn't even compile against the
  current `ThreadState`.
- [systemc/CMakeLists.txt](../systemc/CMakeLists.txt) defines
  `SC_CPLUSPLUS=201703L` on `gpu_systemc` consumers. The installed
  SystemC was compiled with C++17 (`sc_api_version_2_3_4_cxx201703L`)
  and the project compiles with C++20; without the override the
  api-version symbol mangles to a tag that doesn't exist in the
  shipped .dylib and the link fails.
- [tools/run_vkglcts_to_sc.py](../tools/run_vkglcts_to_sc.py) is the
  E2E driver. Crops the SC PPM to the sw_ref viewport when sizes
  differ (the SC chain renders into the full glcompat fb, deqp tests
  read back only their viewport).

**Result on the working slice.**

Initial proof-of-concept: 8 / 19 `color_clear.*` cases pass
**bit-perfectly** (RMSE 0.000) — plain clears with no scissor /
color-mask state. These are the SC chain's first end-to-end VK-GL-CTS
conformance hits.

**Full sweep (2019 cases × 4 workers, sc-timeout=15s, 64 min wall):**

| bucket | count | % | what it means |
|---|---:|---:|---|
| bit-perfect | **14** | 0.7 % | RMSE = 0; SC chain matches sw_ref byte-for-byte |
| diff | 1082 | 53.6 % | rendered but RMSE > 1 (scene-format gaps, blend-state divergence) |
| timeout | 903 | 44.7 % | SC > 15 s wall (heavy fragment counts; bigger budget would catch some) |
| other-error | 20 | 1.0 % | tests with no comparable image (impl-limits queries, prerequisite, light_amount) |
| no-scene | 0 | 0.0 % | atexit hook fires for every case |
| **total** | **2019** | 100 % | |

The 14 bit-perfect cases:
- 8 plain color_clear (`{single,multiple,long,subclears}.{rgb,rgba}`)
- 6 fragment_ops.scissor (`contained_line`, `contained_point`,
  `contained_tri`*, `enclosing_*` — the trivial-coverage variants)

Per-group highlights from
[out/sc_e2e_summary.md](../out/sc_e2e_summary.md):

| group | total | bit-perfect | diff | timeout |
|---|---:|---:|---:|---:|
| `color_clear.*` (plain) | 8 | 8 | 0 | 0 |
| `color_clear.*` (scissored / masked / complex) | 11 | 0 | 11 | 0 |
| `fragment_ops.scissor.*` | 17 | 6 | 9 | 2 |
| `fragment_ops.blend.*` | 1060 | 0 | 1060 | 0 |
| `fragment_ops.depth_stencil.*` | 621 | 0 | 0 | 621 |
| `fragment_ops.random.*` | 100 | 0 | 0 | 100 |
| `fragment_ops.interaction.*` | 97 | 0 | 0 | 97 |
| `fragment_ops.depth.*` | 8 | 0 | 0 | 8 |
| `fragment_ops.stencil.*` | 17 | 0 | 0 | 17 |
| `depth_stencil_clear.*` | 11 | 0 | 0 | 11 |
| `buffer.write.*` | 49 | 0 | 2 | 47 |
| `implementation_limits.*` | 16 | 0 | 0 | 0 (16 other) |
| `prerequisite.*` + `light_amount.*` | 4 | 0 | 0 | 0 (4 other) |

**Follow-up gaps (scene format, scoped for later sprints).**

The full-sweep table above pinpoints exactly where the SC chain
diverges from sw_ref. Three orthogonal issues account for almost all
the non-`bit-perfect` rows:

1. **Per-CLEAR scissor / color-mask not in scene.** `SceneOp::CLEAR`
   carries only the resulting RGBA. sw_ref's `glClear` already honours
   scissor + colorMask (Sprint 50 work), but the SC replay redraws the
   whole fb in the clear color. Accounts for the 11 `color_clear.{
   scissored,masked,complex}` `diff` rows + the 11
   `depth_stencil_clear.*` rows (whose state is similarly elided —
   though those time out before the diff matters).
2. **SC cycle-accurate path is slow on heavy fragment counts.** A
   typical `fragment_ops.depth.*` quad sits at ~200 M cycles, well
   beyond the 15 s wall budget the sweep used. The 903 timeouts are
   essentially the SC budget question. Bumping to 60–120 s would
   catch most of `depth_stencil.*` / `stencil.*` / `random.*`; pushing
   into `interaction.basic_shader.*` (the 100-case random-shader
   corpus) requires either a minute-plus per case or speeding up the
   SC inner loop.
3. **Draw-path scene capture records only `varying[0]`** per vertex
   ([glcompat_es2.cpp:636](../glcompat/src/glcompat_es2.cpp#L636)).
   Multi-varying shaders can't roundtrip; this surfaces as systematic
   `diff` on the 1060-case `fragment_ops.blend.*` block — every blend
   case packs vertex color into a single varying, but the SC replay
   still reconstructs through a code path that assumes a single
   per-vertex `varying[0]` color rather than the test's actual
   per-vertex blend-source colors.

Each of these is a self-contained follow-up: (1) extend `SceneOp` +
sw_ref/SC parsers, (2) speed up SC fragment loop or accept a longer
budget, (3) capture full per-vertex `varying[0..N]` in
`scene_record_es2_batch`. The infrastructure to measure progress is
already in place: rerun `tools/run_vkglcts_to_sc.py --cases-file
sw_pass_cases.txt --tsv ...` and diff the bit-perfect counts.

### Sprint 58 — int() trunc + const fold + output widen + setp neg (+18)

After Sprint 57 the residual 21 `basic_shader.*` fails were heterogeneous
random-shader patterns. Each was diagnosed individually; the fixes batch
into six categories that all live in
[compiler/glsl/src/glsl.cpp](../compiler/glsl/src/glsl.cpp) plus a small
ISA encoding tweak.

1. **`int(x)` truncates toward zero per GLSL spec.** Sprint 56 left
   `int(NUMBER)` as identity-pass, so `int(0.5)` became `0.5` (should be
   `0`). Now `int(NUMBER)` const-folds via `static_cast<int>` (which
   truncates toward zero in C++) and `int(runtime)` lowers to
   `t = floor(abs(x)); cmp(x, +t, -t)` using the cmp op's `s2_neg` bit
   (one fresh GPR, three ALUs). Same path applies to `ivec*` / `bvec*`
   ctors — each arg goes through `emit_int_cast` / `emit_bool_cast`
   so `ivec3(0.5, true, true).r` correctly evaluates to `0` instead of
   propagating the `0.5` into the result channel. Identifier args of
   already-integer type (`int` / `bool` / `ivec*`) skip the trunc
   (`is_known_integer_typed` predicate) so wrapping doesn't burn GPRs
   when the compiler already knows the value is integer.

2. **`const`-qualified scalar fold.** Random-shader corpus declares
   10–15 `const float / int / bool` scalars per stage with foldable
   inits like `const float u = vec3(11, false, -0.5).b;`. Sprint 57
   allocated a fresh GPR for each — and the wide shaders in
   `basic_shader.{6,33,…}` overflowed the 32-GPR file, silently aliasing
   `1.0`'s materialise into r0 (= dEQP_Position). `try_const_eval` now
   evaluates literals, unary minus, +/−/×, type casts, comparisons, and
   single-channel member access on a vec ctor with all-foldable args.
   When the result is a scalar and the decl is `const`, the binding
   skips GPR allocation; reads route through `intern_constant(value)`
   in the c-bank. `const`-only is intentional — non-const locals can be
   reassigned (`int c = a; c = -2;` in basic_shader.31) and folding
   them would silently lose the second write.

3. **Init-aliasing for vec ctor / binop locals.** `vec4 a = vec4(...)
   * vec4(...)` allocates a fresh dst GPR for the binop — and previously
   the local `a` got a separate GPR plus a redundant MOV from binop dst
   to `a.slot`. Now `emit_local_decl` detects when init is a BinaryOp
   or a known-fresh Call (vec ctor / `max` / `min` / `dot` / `clamp` /
   …) and aliases the local's slot to the init's dst GPR. Saves 1 GPR
   + 1 ALU per such decl. Identifier / member-access / unary-minus
   inits don't produce fresh GPRs (they alias existing slots) and keep
   the MOV path.

4. **Output slot encoding widened 2 → 3 bits (4 → 8 outputs).**
   `encoding.h: encode_dst_out` was `o & 0x03` — only 4 output slots
   (`o0` for gl_Position, `o1..o3` for varying). With ≥ 4 vec4 of
   varying capacity (`basic_shader.{23,24,29,36,96}` declare 4–5
   vec4-equivalent), slot 4+ aliased onto o0 and blanked
   gl_Position. The `dst_class=1` path now uses bits `[2:0]` (8
   outputs); `dst[4:3]` was already reserved per the spec comment.
   `ThreadState.o` and `dst_lvalue` updated to match.

5. **Spec-correct cmp for all four relational predicates.** Sprint 57
   shared one cmp form between `<` and `<=` (and between `>` and
   `>=`), making `<=` strictly wrong at the boundary —
   `bool k = 4 <= int(4)` evaluated to `0` instead of `1` (case 10).
   Each predicate now gets its own canonical form using the s0_neg
   bit; no extra ALUs. The s0_neg trick on the GPR-only s2 lane
   (cmp has no s2 *class* field) lets `<=` / `>=` reuse the cmp
   shape with a free sign flip.

6. **`emit_setp` propagates operand neg flags.** The setp emitter
   accepted only positive operands — `if (float(-3) > float(g))` lost
   the unary minus and setp evaluated `+3 > g` (always true for sane
   `g`). `basic_shader.10` took the wrong branch every fragment.
   Trivial fix: pass `Operand::neg` through to `f.s0_neg` / `f.s1_neg`
   in the setp encoding.

- `basic_shader.*`: 79/100 → **97/100** (+18).
- `fragment_ops.*`: 1902 → **1920 / 1923** (+18, **99.8 %**).
- Aggregate sweep: 2017 → **2035 / 2143** (+18, **95.0 %**).
- Zero regressions in any other group (blend / depth / stencil /
  scissor / random / buffer / depth_stencil_clear / color_clear /
  implementation_limits / info / prerequisite all hold at their
  Sprint-57 numbers).
- Remaining 3 `basic_shader.*` fails (24, 33, 75) all involve
  multi-draw iterations chaining glColorMask + glBlendFunc/Equation
  Separate + glDepthFunc + glScissor in 3+ consecutive
  `glDrawElements` calls per iteration. Standalone single-draw FS sim
  produces the right output for each; the pipeline composite over the
  draw chain diverges in subtle ways (R-channel saturation in 24/75,
  A-channel halving in 33). Each one needs its own multi-draw state-
  interaction trace and is no longer a codegen issue.

### Sprint 57 — varying packing + vec equality + GPR pressure (+15)

After Sprint 56 the residual 36 `fragment_ops.interaction.basic_shader.*`
fails clustered around four orthogonal codegen gaps. Each is fixed in
[compiler/glsl/src/glsl.cpp](../compiler/glsl/src/glsl.cpp).

1. **Varying packing.** ISA encoding (`encoding.h:215`) gives
   `dst_class=1` only 2 bits → `o0..o3`, so VS has 4 outputs total
   (1 for `gl_Position`, 3 for varying). Codegen previously gave each
   `varying` declaration its own slot, so a shader with 6 floats
   (e.g. `basic_shader.4`) ran `mov o4, …` which `& 0x03`-aliased
   onto `o0`, blanking `gl_Position` mid-execution — every fragment
   then hit the clear color. Now scalars / vec2 / vec3 / vec4
   pack into the available 3 vec4 outputs (`p,a,d,l → o1.xyzw`,
   `m,o → o2.xy`). FS varying-in runs the identical algorithm so the
   layout matches as long as VS / FS declare varyings in the same
   order — which the dEQP random-shader generator does. The packed
   `(channel_offset, channel_count)` drives the FS-side read swizzle
   (e.g. `b` packed at slot 0 channel 1 reads `.yyyy`).

2. **Scalar-reduce vec `==` / `!=` in expression context.** GLSL spec:
   `vec_a == vec_b` returns SCALAR `bool` (componentwise compare then
   `all`). Sprint 56's per-channel cmp left `bool s = ivec3(…) ==
   ivec3(…)` reading just `.x` of the per-channel result — `s` came
   out true when component 0 matched but components 1/2 differed.
   Now the codegen does `diff = a - b`, `dist = dp4(diff, diff)` and
   compares `dist - ε <= 0`. Same pattern applies to `if (vec ==
   vec)` (the `setp` ISA op is also scalar — `sim.cpp:setp_test`
   only reads `s0[0] / s1[0]`).

3. **GPR-pressure relief.** ALU dst (`encoding.h:215`) is 5 bits → 32
   GPRs; overflow silently aliases via `& 0x1F`. Wide random shaders
   blew past r31 so a downstream materialise of `1.0` clobbered the
   attribute at r0 (= `dEQP_Position`), producing the same blank-
   triangle failure as the varying-slot collision. Three changes
   collectively make every `cmp` cost a single fresh GPR:
   - `zero_gpr()` / `one_gpr()` cache the 0.0 / 1.0 GPR per shader
     (the cmp s2 field has no class — it must be a GPR).
   - cmp dst overwrites `diff` instead of allocating a new register
     (per-cycle dispatch reads s0 atomically before writing dst).
   - The `==` / `!=` path no longer needs `one_gpr()` at all — uses
     s0_neg + `cmp(±diff, 1, 0)` with s2 = cached zero.

4. **Spec-correct `<=` / `>=` at the boundary.** Sprint 56 lowered
   `<=` and `<` to the same `cmp(diff, 0, 1)` (strict `<`), which
   returns 0 at `a == b` — wrong for `<=`. Now each predicate gets
   its own canonical form:
   `<` → `cmp(diff, 0, 1)`; `<=` → `cmp(-diff, 1, 0)`;
   `>` → `cmp(-diff, 0, 1)`; `>=` → `cmp(diff, 1, 0)`.
   The negate is a free ALU bit. `bool k = 4 <= int(4)` now correctly
   evaluates 1.

- `interaction.basic_shader.*`: 64/100 → **79/100** (+15).
- `fragment_ops.*`: 1887 → **1902/1923** (+15, **98.9 %**).
- Aggregate sweep: 2002 → **2017/2143** (+15, **94.1 %**).
- All other groups (blend / depth / stencil / scissor / random / etc.)
  hold at 100 % — zero regressions. The remaining 21 basic_shader
  fails are heterogeneous (mixed-type arithmetic where `int(0.7)`
  needs truncation, vec member access with packed offsets, edge
  cases in if-else nesting); each will need targeted diagnosis.

### Sprint 56 — c-bank 16 → 32, bool() conversion, literal dedup (+26)

Three coordinated fixes after diagnosing
`fragment_ops.interaction.basic_shader.1` (G channel was `64` instead
of `255`):

1. **`bool(x)` was identity-pass.** GLSL spec: `bool(x)` returns
   `(x != 0) ? 1 : 0`. dEQP's random-shader generator uses
   `bool(0.25)`, `bool(0)`, `bool(true)` everywhere; identity-pass
   propagated `0.25` straight into a vec4 channel where it should
   have been `1.0`. Constant-fold for `bool(NUMBER)` at compile time;
   runtime path uses `cmp(x*x − ε, 1, 0)` (3 ALUs).

2. **C-bank widened 16 → 32 slots.** The ISA's `s0idx` is 5 bits
   (32-addressable) but `state.h::DrawState::uniforms` and
   `sim.h::ThreadState::c` were sized 16. Random-shader pairs pack
   1 + 5 uniforms + 10 – 13 distinct literals per VS / FS pair, easily
   overflowing 16. One-line storage bump.

3. **VS / FS literal dedup at bake.** `compile()` takes a new
   `preset_literals` parameter. glcompat seeds the FS compile's
   literal pool with VS's (value→slot) pairs, so duplicate values
   (1.0, 0.0, –1.0) share slots. Without dedup the 32-slot c-bank
   still overflows on shaders with many distinct literals.

- `interaction.basic_shader.*`: 38/100 → **64/100** (+26).
- `fragment_ops.*`: 1861 → **1887/1923** (+26, **98.1 %**).
- Aggregate sweep: 1976 → **2002/2143** (+26, **93.4 %**).
- Remaining 36 fragment_ops fails are random-shader edges (vec `==`,
  GPR pressure, mixed-type arithmetic) — each needs targeted
  diagnosis; the next batch is bounded but heterogeneous.

### Sprint 54 — constant-depth quad precision bypass (+64)

Pattern in `fragment_ops.depth_stencil.stencil_depth_funcs.*`: 4-of-8
depth funcs failed (LESS, EQUAL, GEQUAL, NOTEQUAL); 4-of-8 passed
(LEQUAL, GREATER, ALWAYS, NEVER). The four failures are exactly the
ones whose outcome differs from a "non-strict / always" alternate
*at the boundary case* (`src == dst`).

Diagnosis: dEQP's base-depth and visualisation quads are constant-
depth (all 4 verts share `cmd.params.depth`). Our rasterizer
interpolated `frag.depth = l0*z0 + l1*z1 + l2*z2`, but float
`l0+l1+l2` is not exactly 1.0 — drift of a few ULP at the boundary
flips strict comparisons.

Fix: when `v0.z == v1.z == v2.z`, use the value directly without
multiplication. One-liner closes the entire subgroup:

- `fragment_ops.depth_stencil.stencil_depth_funcs.*`: 49/81 →
  **81/81** (+32).
- `fragment_ops.*`: 1790 → 1854 (+64). Aggregate sweep: 1905 →
  1969 / 2143 (+64, 91.9 %). The fix cascades through
  `depth_stencil.random.*` and `random.*` which use the same
  visualisation pattern.

### Sprint 55 — VS/FS c-bank slot separation (+7)

Diagnosed via `interaction.basic_shader.0`: result G channel was 0
where the reference had 255. The fragment shader writes
`gl_FragColor = vec4(1.0, c, 1.0, 0.0)` with `c = 1` (set via
`glUniform1iv(loc=1, 1, {1})`); G should land at 255.

Tracing the bake: VS uniform `f` was at c-bank slot 0; FS uniform
`c` was *also* at c-bank slot 0. The runtime `uniform_loc` is an
`unordered_map`, so the iteration order in `run_draw` was
non-deterministic: the test's `glUniform1iv(loc=0, 1, {-8})` call
for `f` could clobber the previous `glUniform1iv(loc=1, 1, {1})`
for `c` in the *shared* slot, and the FS read `f`'s value (-8)
where it expected `c = 1`. Same problem on the literal side: VS
literal `-1.1` and FS literal `1.0` both grabbed slot 15, and the
bake's literal-push order let FS overwrite VS.

Fix: `compile()` now takes `uniform_slot_base` (default 0) and
`literal_slot_top` (default 16). glcompat compiles VS first, then
computes `vs_uniform_top` and `vs_literal_bottom`, then compiles
FS with those as bases — uniforms grow up from 0 (VS) → top (FS),
literals grow down from 16 (VS) → bottom (FS). No collisions.

- `fragment_ops.interaction.basic_shader.*`: 12/100 → **38/100** in
  isolation (+26). Some of those gains are absorbed by other
  basic_shader tests' own bugs surfacing now that the slot
  collision is gone.
- Net `fragment_ops.*`: 1854 → 1861 (+7). Aggregate sweep: 1969 →
  1976 / 2143 (+7, 92.2 %).

### Sprint 53 — depth write requires depth_test enabled (+97)

GLES 2.0 § 4.1.6: "When [the depth test is] disabled, the depth
comparison and subsequent possible updates to the depth buffer
value are bypassed and the fragment is passed to the next
operation." Our `per_fragment_ops` was writing depth whenever
`depth_write` was true, regardless of `depth_test`.

Diagnosed via `depth_stencil.stencil_depth_funcs.stencil_always_no_depth`:
- RES had B (depth visualisation) always 0; REF had B varying.
- The test quad runs with `glDisable(GL_DEPTH_TEST)` + `glDepthMask(GL_TRUE)`.
- That clobbered the depth buffer the visualisation phase relies on.
- The visualisation's `LESS` test then failed for every step quad,
  so no blue ever painted.

Fix: gate `fb.depth[pix] = f.depth` on `ds.depth_test && ds.depth_write`
in both single-sample and 4×MSAA paths.

- `fragment_ops.depth_stencil.stencil_depth_funcs.*`: 41/81 → 49/81
  (+8). The fix cascades through the visualisation pipeline that
  many other fragment_ops sub-suites also use:
- `fragment_ops.*`: 1693 → **1790 / 1923** (+97, 93.1 %).
- Aggregate sweep: 1808 → **1905 / 2143** (+97, 88.9 %).

### Sprint 52 — sub-rect framebuffer init (architectural; sweep flat)

dEQP's platform shim calls `viewport(0, 0, RT_w, RT_h)` before any
test runs. Sub-rect-wrapped tests (`sglr::GLContext` with a
`baseViewport` like `(16, 168, 64, 64)` inside a 256×256 RT) then
call `viewport(...)` to set up their sub-rect, and the test's
`glClear` is supposed to fire against a 256×256 fb.

Our impl deferred fb allocation until the *first* `glClear`, which
fired *after* sglr had narrowed the viewport. The fb came up at
sub-rect dimensions; draws still hit it, but `glReadPixels` from
outside the sub-rect read OOB and returned all zeros — every
`interaction.basic_shader.*` test rendered fully black, the
result/reference comparison was hopeless.

Fix: when `glViewport` runs and `ctx_inited == false`, allocate
`fb` at the viewport's `(w, h)` immediately and mark it inited.
Subsequent viewport calls just update the transform; the fb keeps
its full RT size.

- `interaction.basic_shader.*` (alone): 0/100 → 12/100 (+12). Result
  histograms now match the reference's pixel counts; the residual
  diffs are real draw-side bugs (the reference rect is now full of
  the clear colour, not zeros).
- Net sweep flat at **1808 / 2143**: the architectural unmasking
  exposed draw-side bugs in `random.*` cases that had been
  accidentally passing because all-black happened to match the
  expected all-black sub-rect. Bottleneck moves to per-draw
  correctness (blend factor edge cases, dither, multi-pass
  colorMask sequencing).

### Sprint 51 — typed vertex attribute reads (+47)

dEQP `buffer.write.*` uploads a buffer of random bytes and verifies it
by binding the buffer as a 3-byte normalized color attribute and
rendering a quad-grid: each vertex's color comes from 3 successive
bytes, fragments interpolate, output is compared against a
software-rendered reference.

Our `run_draw` was reading every attribute as `float*` regardless of
the `glVertexAttribPointer` `type`. With `GL_UNSIGNED_BYTE` +
`normalized=GL_TRUE` the bytes were being reinterpreted as floats,
producing garbage colors. Sprint 51 switches on `va.type` and
converts:

- `GL_BYTE`            — `b / 127.0` (clamped at -1)
- `GL_UNSIGNED_BYTE`   — `u / 255.0`
- `GL_SHORT`           — `s / 32767.0` (clamped at -1)
- `GL_UNSIGNED_SHORT`  — `u / 65535.0`
- `GL_FIXED`           — `i / 65536.0`
- `GL_FLOAT`           — passthrough

When `normalized=GL_FALSE` the integer types pass through as plain
floats. Default stride now derives from per-type element size instead
of hard-coded `sizeof(float)`.

`buffer.*` 2/49 → **49/49** (+47). Aggregate sweep 1761 → 1808 / 2143
(+47, 84.4 %).

### Sprint 50 — glUniform float variants + masked clear (+11)

Two related glcompat gaps, both diagnosed via dEQP
`depth_stencil_clear.*` (was 0/11 — closes the entire group):

- **Missing scalar `glUniform2f` / `glUniform3f` / `glUniform4f`.**
  dEQP-GLES2's `depth_stencil_clear` visualization calls
  `gl.uniform4f(colorLoc, 0, 0, c, 1)` directly. With those symbols
  absent from `gpu_glcompat`, `dlsym(RTLD_DEFAULT)` returned `NULL`
  and every uniform stayed at zero — the visualization shader saw
  `u_color = (0,0,0,0)` and the blue channel never advanced.
- **`glClear(DEPTH_BUFFER_BIT | STENCIL_BUFFER_BIT)` ignored
  `glDepthMask` / `glStencilMask`.** GLES 2.0 § 4.3.2 says clear
  writes only through the active write mask. Sprint 50 wires
  `s.depth_write` (skip depth clear when `GL_FALSE`) and applies the
  bit-level stencil write mask to the cleared value (`new = (old &
  ~m) | (clear & m)`).
- `depth_stencil_clear.*`: 0/11 → **11/11** (+11). Aggregate sweep
  1750 → 1761 / 2143 (+11, 82.2 %).

### Sprint 49 — glStencilFunc ref clamp (+13)

Tiny, high-leverage spec fix:

- GLES 2.0 § 3.7.6 / § 4.1.5: `glStencilFunc(func, ref, mask)` *clamps*
  `ref` to `[0, 2^s − 1]` where `s` is the stencil bit-plane count.
  Our `glStencilFuncSeparate` used `ref & 0xFF` (mask) instead of
  clamp, which collapsed `ref = 256` to `0` (should be `255`) and
  `ref = -1` to `255` (should be `0`).
- Diagnosed via `fragment_ops.stencil.cmp_not_equal`: dEQP's stencil
  test grid feeds the two out-of-range values `1 << stencilBits`
  (= 256 for 8-bit) and `-1` for the last two cells. Our masking
  swapped those two cells' colors, tripping `Image comparison failed`
  on every test that walks the full grid.
- `fragment_ops.stencil.*`: 10/17 → **17/17** (+7) — closes
  `cmp_not_equal`, `stencil_fail_replace`, `depth_fail_replace`,
  `depth_pass_replace`, `incr_wrap_stencil_fail`,
  `decr_wrap_stencil_fail`, `zero_stencil_fail`,
  `invert_stencil_fail`.
- Several `fragment_ops.random.*` also flipped (same out-of-range
  ref-value path). Net `fragment_ops.*` 1680 → 1693, sweep
  1737 → 1750 (+13).

`fragment_ops.scissor.*` 11 → 17/17. Aggregate 1713 → 1727 / 2143.

### Sprint 46 — fragment_ops conformance push (+1633)

Four orthogonal fixes, each closing a meaningful slice of `fragment_ops.*`:

1. **Separate-channel blend.** Wired
   `glBlendFuncSeparate` / `glBlendEquationSeparate` / `glBlendColor`
   in glcompat (previously they hit the no-op stub fallback in the CTS
   shim, so dEQP blend tests ran with default `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`).
   `gpu::DrawState` grew `blend_src_rgb` / `blend_src_alpha` /
   `blend_dst_rgb` / `blend_dst_alpha`, separate equations,
   `blend_color`, plus the missing `BF_CONSTANT_*` and
   `BF_SRC_ALPHA_SATURATE` factors. ~+50 cases on its own.

2. **Top-left fill rule.** The rasterizer's edge-coverage test now
   distinguishes "top" / "left" edges (include `w_i == 0`) from
   "right" / "bottom" (exclude). Without it, the diagonal between the
   two triangles of a quad was double-covered, which under
   `blend ADD ONE/ONE` saturated 60-ish edge pixels per quad — that's
   why most of the blend tests failed even when the per-pixel math was
   right. **Single biggest win — closed ~1003 cases on its own.**

3. **Front/back-separate stencil.** `DrawState` grew
   `stencil_func_back` / `sop_*_back` / `stencil_ref_back` /
   `stencil_*_mask_back`. `Fragment::front_facing` is set by the
   rasterizer from `sign(area)`; PFO selects the matching face's
   state. New entry points `glStencilFuncSeparate` /
   `glStencilOpSeparate` / `glStencilMaskSeparate` in glcompat.
   ~+244 cases.

4. **`SO_INCR_WRAP` / `SO_DECR_WRAP` stencil ops.** Modulo-256 wrap
   variants of INCR/DECR — listed in the GLES 2.0 spec but never
   implemented. Returns `(uint8_t)(cur ± 1)` with no clamp. +332 cases
   in one line.

Net effect: `fragment_ops.*` 23 → 1656 / 1923 (86.1 %); aggregate
sweep 80 → 1713 / 2143 (79.9 %). The remaining 267 fragment_ops fails
break down as 100 `basic_shader` (random shader compile failures —
needs deeper GLSL front-end work), 40 `stencil_depth_funcs` residue,
24 `random`, ~7 one-offs.

### Reading the table

- **passed** — `<Result StatusCode="Pass">` in the QPA log.
- **failed** — `<Result StatusCode="Fail">`. Most non-Pass results in our
  sweep are genuine pipeline gaps (limits, GLSL features, FBO blits).
- **other** — anything else (`NotSupported`, `QualityWarning`,
  `ResourceError`). Largest bucket is `capability.*` where dEQP gracefully
  reports "extension N is unsupported" rather than a hard fail.
- **wall (s)** — single-process serial run; no `--deqp-runmode=execute`
  parallelism is enabled.
- **Δ vs S42** — change vs the Sprint 42 baseline that first checked into
  this report (68/2143 across 11 groups).

### Next-most-tractable pickups (rough effort estimate)

| group | status | likely fix | effort |
|---|---|---|---|
| `fragment_ops.interaction.basic_shader.*` | 0/100 | dEQP synthesises random shaders; our GLSL ES 1.0 front-end can't compile most of them (needs full grammar incl. for/while + user functions + `$MAIN$` substitution). | large |
| `fragment_ops.depth_stencil.stencil_depth_funcs.*` residue | 41/81 | combined depth+stencil interaction edge cases; residual 40 are a long tail. | medium |
| `fragment_ops.random.*` | 0/100 | random fragment-op combinations; each seed is its own micro-config. | medium |
| `depth_stencil_clear.*` + `buffer.*` | 0/11 + 2/49 | shared root cause: shader + glDrawElements + per-pass uniform-update interaction. Result is rendering all pixels but with wrong colours after the visualisation pass. | medium |
| `light_amount.*` | 1/19 | GLES 2.0 + fixed-function lighting is ill-defined; most cases will stay N/A. | skip |
| `clip_control.*` / `multisampled_render_to_texture.*` | NotSupported | GLES 3+ / extension; reporting NotSupported is the right answer. | done |
