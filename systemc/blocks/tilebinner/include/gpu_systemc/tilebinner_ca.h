#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 28 — Phase 2 cycle-accurate TileBinner (BIN_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (TileBinJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Per accepted TileBinJob: walks each triangle's screen-space bbox
// and increments bin_counts[tile_y * grid_w + tile_x] for every
// 16×16 (or job->tile_size) tile the bbox touches. Forwards the
// same pointer downstream.
//
// Timing placeholder: 1 cycle per (triangle × tile_in_bbox). Real
// binner is a 2-stage bbox-snap → tile-list-write pipe, modeled in
// Phase 2.x.
SC_MODULE(TileBinnerCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(TileBinnerCa);
    explicit TileBinnerCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
