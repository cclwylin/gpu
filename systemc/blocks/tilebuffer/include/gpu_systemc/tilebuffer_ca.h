#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 26 — Phase 2 cycle-accurate TileBuffer (TBF_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (TileFlushJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Storage placeholder. The actual on-chip ~64 KB SRAM is modelled by
// the `Context::fb.color_samples / depth_samples / stencil_samples`
// vectors hosted on the host side; this block only stamps a flush
// timing on the path between PFO_ca outputs and RSV_ca.
//
// Timing placeholder: 1 cycle per pixel in the tile (tile_w*tile_h).
// Real TBF: 8-bank SRAM, 4-sample-wide read port for resolve, write
// port from PFO at quad rate. Bank-conflict modeling is Phase 2.x.
SC_MODULE(TileBufferCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(TileBufferCa);
    explicit TileBufferCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
