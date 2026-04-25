#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 22 — Phase 2 cycle-accurate Rasterizer.
//
// Wire shape:
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (RasterJob*)
//   Downstream producer:  frag_valid_o, frag_ready_i, frag_data_o (same pointer)
//
// Per accepted RasterJob: run edge-function rasterisation over the
// triangle list (1× and 4× MSAA paths supported, matching the LT
// implementation), populate `job->fragments` in place, forward
// downstream. Per-pixel-centre barycentric for varying / depth.
//
// Timing placeholder: 1 cycle per emitted fragment. Real coarse +
// fine raster pipe latencies, edge-eval throughput, MSAA sample bank
// conflicts → Phase 2.x.
SC_MODULE(RasterizerCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     frag_valid_o;
    sc_core::sc_in<bool>      frag_ready_i;
    sc_core::sc_out<uint64_t> frag_data_o;

    SC_HAS_PROCESS(RasterizerCa);
    explicit RasterizerCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
