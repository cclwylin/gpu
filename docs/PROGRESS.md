---
doc: PROGRESS
purpose: Open follow-ups (to-do list). Closed items live in `docs/regress_report.md` § per-sprint.
last_updated: 2026-04-28 (Sprint 61 — multi-varying + full blend + per-batch viewport in scene format)
---

# PROGRESS — to-do

Closed sprints are documented in [`regress_report.md`](regress_report.md);
this file tracks only what's pending. Tick items as they ship.

## Hot path — VK-GL-CTS → SC E2E parity (Sprint 59 onwards)

- [ ] **3 fragment_ops.basic_shader fails (24, 33, 75)** — multi-draw
      iterations chaining `glColorMask` × `glBlend{Func,Equation}Separate`
      × `glDepthFunc` × `glScissor` in 3+ consecutive draws per
      iteration. Standalone FS sim produces the right output; divergence
      is in the multi-draw composite. Each needs its own state-
      interaction trace.
- [ ] **903 SC-chain timeouts** in the full E2E sweep
      (`fragment_ops.{depth,depth_stencil,stencil,random,interaction}`).
      `add_zero_zero`-style cases finish in ~2 cycles; depth/stencil
      tests sit at ~200 M cycles each. 60 s budget caught zero of 50
      sampled. Either trim the corpus or speed up the SC inner loop.
- [ ] **Sub-pixel RMSE on `fragment_ops.blend.*`** — Sprint 61 closed
      the gross-logic miss (median RMSE ~110 → 6.2 on a 50-case sample),
      but only 4/50 are bit-perfect. Remainder is rasterizer edge
      precision delta between sw_ref and the SC chain.
- [ ] **`depth_stencil_clear.*` 11 cases** still timeout — Sprint 60
      fixed the scissor/mask gap; depth + stencil clears each run a
      heavy SC inner loop. Tied to the SC-speedup item above.
- [ ] **Viewport size mismatch** when test FB ≠ 256×256 — driver
      auto-crops PPM bottom-left, may misalign for non-(0,0) viewports
      with zero-padded scene.

## VK-GL-CTS coverage (item 11, ongoing)

- [ ] **`functional.shaders.*`** — ~10 k cases. Most need full GLSL
      grammar (loop CF, user functions, `$MAIN$` substitution). Tied to
      item 3 below.
- [ ] **`functional.texture.*`** — ~1 k cases. Skipped from the
      default sweep (heavy). Mip / cube / fbo path needs a full pass.
- [ ] **`light_amount.*`** — 1/19 — fixed-function lighting, low
      priority for v1 (ill-defined on GLES 2.0 contexts).
- [ ] **`clip_control.*` / `multisampled_render_to_texture.*`** —
      reports NotSupported today; extension features, defer to
      post-v1.

## GLSL frontend (item 3)

- [ ] **for / while loops** — random-shader `function` /
      `conditionals` / `loop` glmark2 scenes blocked on this. Single
      front-end push unlocks 3 scenes + a chunk of `shaders.*`.
- [ ] **User function definitions + `$MAIN$` substitution** — same
      group as above.
- [ ] **Mixed-type arithmetic edge cases** — `int(0.7)` runtime trunc,
      member access on packed vec varyings, edge cases in nested
      if/else. Surfaced individually in remaining basic_shader fails.

## SC chain / SystemC (Phase 2 follow-ons)

- [ ] **SC inner-loop speedup** — fragment-heavy tests at ~200 M
      cycles. Either reduce per-cycle work in PFO/RS or skip-ahead on
      idle quads.
- [ ] **Two-sided stencil, polygon offset, depth-bounds** — deferred.
      Polygon offset shows up in dinoshade's sw_ref↔SC RMSE 9.43 (97
      boundary pixels).
- [ ] **Stencil per-sample for sample-shading** — out of scope for v1.
- [ ] **Bitmap font sub-pixel parity** — texenv RMSE 1.60. Needs
      deterministic raster-pos rounding agreement between live
      `glRasterPos` and SC replay.

## SPIR-V path (items 1, 2)

- [ ] **SPIR-V → IR coverage** — OpExtInst (GLSL.std.450),
      OpAccessChain, OpVectorShuffle, OpDot, OpImageSampleImplicitLod,
      control flow.
- [ ] **glslang → SPIR-V → ISA E2E** — Sprint 12 produces SPIR-V,
      Sprint 13 lowers it; gluing under `-DGPU_BUILD_GLSLANG=ON` with a
      real GLSL test still pending.

## FP precision (items 4, 5)

- [ ] **3-ULP HW-aligned LUT polynomials** — Sprint 16 polynomials are
      ~1e-2 to ~1e-3 relative.
- [ ] **sim ↔ sw_ref FP bit-alignment** — both should call the same
      library; sim uses `std::*`, sw_ref uses Sprint-16 polynomials.

## glmark2 (items 6–10, 17)

- [ ] **Asset loaders** — libpng / libjpeg-turbo (in-tree), 3DS / OBJ /
      JOBJ. Required for everything beyond `clear` / hand-built tris.
- [ ] **Upstream scene coverage** — 22 upstream, 2 covered (`clear`,
      `pulsar` via `glmark2.glsl`). Suggested order:
      - **(A)** `build`, `buffer`, `grid` — procedural mesh, no loader.
      - **(B)** `conditionals`, `function`, `loop` — blocked on GLSL
        for/while + non-main fns.
      - **(C)** `texture`, `effect-2d` — needs PNG decoder
        (single-file `stb_image` would do).
      - **(D)** `shading`, `bump`, `refract`, `shadow` — partly (C)
        loaders, partly extra GLSL built-ins.
      - **(E)** `jellyfish`, `ideas`, `terrain`, `desktop` — JOBJ /
        3DS / OBJ + multi-pass + GLES 3 bits.
- [ ] **Link upstream `tests/glmark2/src/main.cpp` + `scene-*.cpp`** —
      blocked on (A) + asset loaders.

## Phase 3 (RTL, driver)

- [ ] **Verilog RTL** — not started.
- [ ] **Linux EGL + GLES 2.0 driver** — not started. Out of scope until
      RTL stabilises.
