#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 27 — Phase 2 cycle-accurate L2 cache.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (MemRequest*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Pass-through placeholder. Real L2: N-way set-associative, 64 B
// lines, write-back, write-allocate; cache-fill from MC; tag/data
// SRAM banks. Modeled as Phase 2.x.
//
// Timing placeholder: hit-latency 4 cycles, miss-latency 0 (we just
// forward to MC); request flag `fault` short-circuits forwarding.
SC_MODULE(L2CacheCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(L2CacheCa);
    explicit L2CacheCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
