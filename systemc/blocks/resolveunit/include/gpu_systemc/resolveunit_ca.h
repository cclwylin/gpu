#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 26 — Phase 2 cycle-accurate ResolveUnit (RSV_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (TileFlushJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Per accepted TileFlushJob: invokes gpu::pipeline::resolve(*ctx)
// which box-filters 4×MSAA samples → 1× RGBA8 per pixel and writes
// fb.color in place. No-op when fb.msaa_4x == false.
//
// Timing placeholder: 1 cycle per pixel of the framebuffer (real
// hardware: pipelined 4-sample-wide datapath at 1 px/cycle).
SC_MODULE(ResolveUnitCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(ResolveUnitCa);
    explicit ResolveUnitCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
