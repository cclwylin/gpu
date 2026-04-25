---
doc: Phase 2 Kickoff
version: 0.2 (draft)
status: in progress (Sprint 18)
owner: E1
last_updated: 2026-04-25
---

# Phase 2 Kickoff — Cycle-Accurate SystemC

Master Plan Phase 2 spans M9–M16 (7 months). Goal: every TLM-LT block
graduates to a **cycle-accurate / pin-level / synthesizable-subset**
SystemC implementation. The Phase 1 LT blocks remain in tree for
spec-vs-impl co-simulation; Phase 2 blocks add a parallel
`*_ca.{h,cpp}` per block behind a CMake option.

> **Glossary note**: in OSCI / Accellera taxonomy "PV" (Programmer's
> View) ≈ LT-equivalent. The Phase-2 work targeted here is strictly
> **CA — Cycle-Accurate** (sc_in/out + SC_CTHREAD + clk + ready/valid).
> We use `_ca` in filenames to be precise. AT (Approximately-
> Timed, TLM-2.0 nb_transport with phases) is a separate intermediate
> abstraction we are NOT pursuing.

This Sprint-18 commit lays the **template** with one block (CP):

- `commandprocessor_ca.{h,cpp}` — `SC_CTHREAD` driven by
  clk + rst_n, ready/valid handshake on the downstream interface,
  no TLM blocking calls.
- Testbench (`test_commandprocessor_ca.cpp`) exercises the
  handshake against a tiny `Sink` consumer.

## Naming + layout convention

Both flavours coexist; the top-level instantiates one or the other
based on a CMake flag (Phase 2.x). File / class suffix maps to the
SystemC abstraction level:

```
systemc/blocks/<blockname>/
  include/gpu_systemc/<blockname>_lt.h    Phase 1  TLM-LT       (b_transport)
  include/gpu_systemc/<blockname>_at.h    (future) TLM-AT       (nb_transport+phases)
  include/gpu_systemc/<blockname>_pv.h    (future) PV           (untimed programmer's view)
  include/gpu_systemc/<blockname>_ca.h    Phase 2  cycle-accurate (sc_signal + CTHREAD)
  src/<blockname>_lt.cpp
  src/<blockname>_ca.cpp
```

Class names mirror the file suffix: `CommandProcessorLt`,
`CommandProcessorCa`, etc.

## Wire-level convention (template)

Every cycle-accurate block exposes:

```cpp
sc_in <bool>          clk;
sc_in <bool>          rst_n;
// Producer-side downstream:
sc_out<bool>          *_valid_o;
sc_in <bool>          *_ready_i;
sc_out<uint64_t|...>  *_data_o;       // payload pointer or fixed-width bits
// Consumer-side upstream (mirror):
sc_in <bool>          *_valid_i;
sc_out<bool>          *_ready_o;
sc_in <uint64_t|...>  *_data_i;
```

Handshake rule: producer drives `valid`+`data` for as long as it has
data; consumer drives `ready` when it can consume. Transfer happens on
the clock edge where both are high. Same as AXI-stream and
ready/valid in any standard pipeline.

`SC_CTHREAD` synchronous to `clk.pos()`; `reset_signal_is(rst_n, false)`
i.e. active-low async reset deassertion sync. Matches
[`docs/coding_style.md §4.3`](coding_style.md).

## Migration order (proposal — Phase 2 sprints)

Each sprint converts the named block from TLM-LT to cycle-accurate.

| Sprint | Block | Reason for ordering |
|---|---|---|
| 19 | CP done; **VF** | Linear-pipeline producer side |
| 20 | **SC** | Largest block; must land early to unblock real timing |
| 21 | **PA** | Small, mostly arithmetic |
| 22 | **RS** | Heavy logic; per-pixel rate model |
| 23 | **TMU** + **L1 Tex$** | Adds memory subsystem entrypoint |
| 24 | **PFO** | depth/stencil/blend timing |
| 25 | **TBF** + **RSV** | tile buffer SRAM model + resolve |
| 26 | **MMU** + **L2** + **MC** | end of internal datapath |
| 27 | **CSR** + **PMU** | non-pipeline sidebands |
| 28 | Co-sim of full cycle-accurate chip vs sw_ref / TLM | Phase 2 exit gate |

## Co-simulation strategy

- Each block has both LT and cycle-accurate implementations.
- Top-level CMake flag selects the build
  (`-DGPU_SYSTEMC_FLAVOR=lt|ca`).
- Reference runs (sw_ref ↔ TLM-LT framebuffer pixel-exact) are the
  Phase-1 invariant. The cycle-accurate chip must produce identical
  pixels to LT chip on the conformance corpus, plus pass cycle-count
  regression caps.

## Phase 2 exit criteria (Master Plan §C)

- All blocks cycle-accurate.
- Co-sim chain `cp.cmd_in → … → fb` matches sw_ref FB to RGBA8 bit
  exactness on the conformance scene set.
- Cycle counts within ±5% of design targets per the architecture spec.
- Coverage thresholds met (line 100% / toggle 98% / functional 100%).

## Out of scope for Sprint 18

- Wiring the cycle-accurate CP into `gpu_top` (a Sprint 19+ task once
  VF also has a cycle-accurate variant; otherwise the chain is
  half-LT half-cycle-accurate which doesn't serve a purpose).
- Rich timing model (latency / FIFOs sized per spec). The Sprint-18 CP
  does back-to-back ready/valid; that's a placeholder.
- Coverage / formal hooks (Phase 2.x).
