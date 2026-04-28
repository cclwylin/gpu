# Regression report

Last updated: **2026-04-28 (M4 Mac mini)** — see § Sweep run notes.

| metric                              | result |
|---|---:|
| In-tree ctest (`build-arm64`)       | 56 / 56 PASS |
| VK-GL-CTS `sw_ref` sweep            | 2035 / 2143 (95.0 %) |
| VK-GL-CTS → SC chain bit-perfect    | 80 / 1985 (4.0 %) |
| 49 GL example regress (sw_ref vs SC)| 45 / 49 RMSE < 1.0 (92 %) |

`tools/run_vkglcts_sweep.py` produces the sw_ref column; `tools/run_vkglcts_to_sc.py
+ tools/aggregate_sc_e2e.py --top-level` produce the SC parity column. Both feed
through the same `gpu_glcompat` shim into either `gpu_sw_ref` (CPU golden) or the
SystemC cycle-accurate chain.

## VK-GL-CTS per group

`sw_ref` column: pass count out of total cases in that group. SC columns split the
sw_ref-passing cases by what the SC chain replay produces (bit-perfect / non-zero
RMSE / timeout / hard error).

| group                                       | sw_ref pass | SC bit-perfect | SC diff (RMSE>0) | SC timeout | SC error |
|---|---:|---:|---:|---:|---:|
| `info.*`                                    | 6 / 6       |   — |    — |    — | — |
| `capability.*`                              | 10 / 88¹    |   — |    — |    — | — |
| `functional.prerequisite.*`                 | 3 / 3       |   0 |    0 |    0 | 3² |
| `functional.color_clear.*`                  | 19 / 19     |  **19** |   0 |    0 | 0 |
| `functional.depth_stencil_clear.*`          | 11 / 11     |   0 |    0 |   11 | 0 |
| `functional.implementation_limits.*`        | 16 / 16     |   0 |    0 |    0 | 16² |
| `functional.buffer.*`                       | 49 / 49     |   0 |    2 |   47 | 0 |
| `functional.fragment_ops.*`                 | 1920 / 1923 |  61 | 1014 |  812 | 0 |
| `functional.clip_control.*`                 |  — / 8³     |   — |    — |    — | — |
| `functional.light_amount.*`                 |  1 / 19⁴    |   — |    — |    — | — |
| `functional.multisampled_render_to_texture` |  — / 1³     |   — |    — |    — | — |

Footnotes:
1. `capability.*` 78 of the 88 are reported as `NotSupported` / `QualityWarning`,
   not failures — sw_ref doesn't claim those capabilities.
2. `prerequisite.*` (3) + `implementation_limits.*` (16) are pure state queries —
   they don't draw, so there's no scene to feed the SC chain. Counted as "error"
   (no scene).
3. `clip_control` and `multisampled_render_to_texture` are GLES 3.0+ / extension
   features sw_ref doesn't claim — NotSupported.
4. `light_amount` exercises fixed-function lighting — not in our GLES 2.0 scope;
   one trivial 0-light case happens to pass.

Bit-perfect breakdown: **`color_clear` 19/19 (100 %)** — the only group that's
fully closed end-to-end. `fragment_ops` 61/1887 bit-perfect with 1014 cases
rendering but RMSE > 0; the remaining 812 timed out at the 15-second SC-chain
budget. `buffer.write.*` random vertex sets dominate the timeout column.

## 49 GL example regress (`tools/regress_examples.py`)

`sw_ref` is the ground-truth render; `SC paint` is the cycle-accurate chain
running the same `.scene` capture. RMSE column is per-pixel L2 over RGB.

| status | count | examples |
|---|---:|---|
| ✓ RMSE < 1.0     | 45 | most of the 49 (`abgr`, `cube`, `dials`, `glpuzzle`, `glutdino`, `glutplane`, `lightlab`, `movelight`, `scene`, `scube`, `splatlogo`, `stenciltst`, `surfgrid`, `trippy`, …) |
| ≈ RMSE 1–10      |  2 | `texenv` 1.60 (bitmap font sub-pixel jitter), `dinoshade` 9.43 (stencil-shadow without polygon offset) |
| · sw_ref-only    |  2 | `oversphere`, `zoomdino` — SC timeout > 60 s |
| ✗ build/run fail |  0 | — |

Full per-example table (paint counts, cycles, wall ms) is in `out/regress_report.md`
— produced by `tools/regress_examples.py`. Run it with no args to refresh.

## Sprint history (Sprint 42 → 61, +1967 cases)

Long-form per-sprint sections are in `docs/PROGRESS.md`. One-paragraph summary
of the major movers, in commit order:

| sprints  | what shifted                                             | sweep delta |
|---|---|---:|
| 42       | first VK-GL-CTS plumbing — `deqp-gles2` through `gpu_glcompat`. | baseline 68 / 2143 |
| 43–46    | scissored / masked clear, separate-channel blend, top-left fill rule, front/back-separate stencil, INCR_WRAP / DECR_WRAP. | +1647 |
| 47–51    | GLSL parser hardening (`#version`, ivec/bvec, true/false), `glStencilFunc` ref clamp per spec, `glUniform{2,3,4}f` scalar variants + `glClear` honors masks, typed vertex attribute reads. | +85 |
| 52–53    | framebuffer sized at first viewport (was first clear); depth-buffer write gated on `depth_test` enabled (GLES 2.0 § 4.1.6). | +97 |
| 54–56    | constant-depth quad bypass for boundary precision, VS / FS c-bank slot separation, c-bank widened 16 → 32, `bool(x)` conversion + literal dedup. | +97 |
| 57–58    | varying packing into vec4 output slots, vec equality scalar-reduce, GPR-pressure cached `zero_gpr` / `one_gpr`, `int(x)` truncation, output-slot encoding 2-bit → 3-bit (4 → 8 outputs), `setp` neg-flag propagation. | +33 |
| 59–61    | VK-GL-CTS → SystemC E2E pipeline: `glcompat` atexit scene dump → `sc_pattern_runner` replay. Scene format extended for `CLEAR` scissor + lane (Sprint 60 → `color_clear` 19/19 bit-perfect), full blend state, multi-varying capture, per-batch viewport (Sprint 61 → `fragment_ops.blend` median RMSE 110 → 6.2 on a 50-case sample). | (SC track; sw_ref total holds at 2035) |

Pickups, in rough effort order:

1. **`fragment_ops` 1014 SC-diff cases** — sweep produces non-zero RMSE; cluster
   by RMSE histogram, find common scene-format / chain gap, push the "rendered
   but wrong" pile down toward bit-perfect.
2. **SC timeout budget** — 870 / 1985 cases hit the 15-second wall on M4. Lifting
   to 30 s should capture most `buffer.write.*` (47 / 49) + `depth_stencil_clear`
   (11 / 11) without regressing wall time per case (most of those are slow
   scenes, not infinite loops). Run on bigger host where possible.
3. **`fragment_ops` last 3 cases** — `interaction.basic_shader.{24,33,75}` are
   the residue from Sprint 58 — multi-draw blend × depth × color-mask × scissor
   composites where the FS sim is correct standalone but the per-draw composite
   over 3+ draws differs.

## Sweep run notes

This snapshot was produced on the M4 Mac mini (Apple Silicon arm64), build dirs
`build-arm64/` + `build_vkglcts-arm64/`. The reported `sw_ref` numbers are
inherited from the last green Intel iMac sweep (Sprint 58) and reproduce on M4
to within ±1 case (state leak across consecutive `deqp-gles2` calls produces
order-dependent jitter — single-test reruns are stable).

The SC sweep was driven by `tools/run_vkglcts_to_sc.py --workers 4
--sc-timeout 15`, taking ~85 minutes on the M4 mini. Raw TSV at
`/tmp/sc_e2e_v2.tsv`; aggregator output at `out/sc_e2e_summary.md`. The
**80 bit-perfect** total is up from 14 / 2019 at Sprint 59 (+66 from Sprint
60–61's CLEAR-scissor + blend + multi-varying scene-format work).

Build environment caveats specific to M4:
- Brew SystemC 3.0.2 ships no `SystemCLanguageConfig.cmake`. `systemc/CMakeLists.txt`
  Sprint 62 (this session) detects brew's install at `/opt/homebrew` and
  synthesizes a `SystemC::systemc` INTERFACE target — no manual `SYSTEMC_HOME`
  needed.
- The shared exFAT volume between Intel iMac and M4 doesn't preserve POSIX
  exec bits, so every checkout flips `100644 → 100755`. Sprint 62 squashed the
  accumulated mode noise into a single commit (`0b8e00a`) so `git status` stays
  quiet.

## CTest detail

`ctest --test-dir build-arm64 -j4 --output-on-failure` — 56/56 PASS:

| namespace        | count |
|---|---:|
| `compiler.*`     |  7 |
| `conformance.*`  |  3 |
| `glmark2.*`      |  6 |
| `sw_ref.*`       |  7 |
| `systemc.*`      | 22 |
| `vkglcts.*`      |  1 (`vkglcts.gles2.color_clear` when `-DGPU_BUILD_VKGLCTS=ON`) |

The `vkglcts.gles2.color_clear` ctest registers a single-group sanity check
inside the main ctest harness — it runs `deqp-gles2 --deqp-case=color_clear.*`
and asserts ≥19 passes. The full sweep (`run_vkglcts_sweep.py`) is the
non-ctest counterpart.
