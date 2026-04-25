#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 24 — Phase 2 cycle-accurate TextureUnit (TMU_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (TextureJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o  (same ptr)
//
// Per accepted TextureJob: walk requests[i], call gpu::sample_texture
// against job->tex, push the resulting RGBA into job->samples in
// order. Forward the same pointer downstream.
//
// Timing placeholder: 1 cycle per request (real TMU pipe: 4-stage
// addr-gen → tag-check → decode → bilinear filter; bilinear takes 1
// cycle, trilinear 2 — full timing in Phase 2.x).
SC_MODULE(TextureUnitCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(TextureUnitCa);
    explicit TextureUnitCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
