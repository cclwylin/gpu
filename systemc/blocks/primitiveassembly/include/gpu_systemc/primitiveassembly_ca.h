#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 21 — Phase 2 cycle-accurate PrimitiveAssembly.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (PrimAssemblyJob*)
//   Downstream producer:  tri_valid_o, tri_ready_i, tri_data_o (same pointer)
//
// Per accepted job: run perspective-divide + viewport + back-face cull
// over `job->vs_outputs` (3 vec4 per vertex), populate `job->triangles`
// in place, then forward downstream. Mirrors the LT implementation so
// the LT and CA paths produce identical screen-space triangles for the
// same input.
//
// Timing placeholder: 2 cycles per emitted triangle (placeholder for
// the real per-tri pipe latency including clipping; will tighten once
// clipping arithmetic is modeled).
SC_MODULE(PrimitiveAssemblyCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     tri_valid_o;
    sc_core::sc_in<bool>      tri_ready_i;
    sc_core::sc_out<uint64_t> tri_data_o;

    SC_HAS_PROCESS(PrimitiveAssemblyCa);
    explicit PrimitiveAssemblyCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
