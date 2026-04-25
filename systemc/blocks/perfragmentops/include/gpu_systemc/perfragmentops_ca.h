#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 25 — Phase 2 cycle-accurate Per-Fragment Operations (PFO_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (PfoJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Per accepted PfoJob: invokes gpu::pipeline::per_fragment_ops on
// (*ctx, *quad). The Quad is consumed in place: framebuffer state
// (color/depth/stencil) inside ctx mutates as a side-effect. Forward
// the same pointer downstream.
//
// Timing placeholder: 1 cycle per fragment in the quad (4 frags ⇒ 4
// cycles). Real PFO pipe is 5-stage (early-Z lookup → stencil-test →
// late-Z → blend-fetch → blend-write); modelled in Phase 2.x.
SC_MODULE(PerFragmentOpsCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(PerFragmentOpsCa);
    explicit PerFragmentOpsCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
