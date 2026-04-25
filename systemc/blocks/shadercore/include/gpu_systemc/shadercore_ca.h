#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 20 — Phase 2 cycle-accurate ShaderCore.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o
//
// `*_data` carries an opaque pointer to a ShaderJob (host-allocated,
// pass-through). For each accepted job:
//   1. Look up the job pointer
//   2. Run gpu::sim::execute on it (mutates job->outputs in place)
//   3. Forward the same pointer downstream
//
// Sprint 20 deliberately is **single-thread (one ShaderJob) per
// accepted transaction**. Real SC has 4 warp slots × 16 lanes with
// scheduler + ALU/SFU/TMU/LSU pipes; that timing model is Phase 2.x.
//
// Timing placeholder: the SC_CTHREAD waits N cycles after job
// acceptance, where N = number of ISA instructions in job->code. This
// approximates "1 cycle per inst" — gross under-count for transcendentals
// (rcp/rsq/exp/log: 6–10 cycles in spec) but a reasonable first cut so
// the rest of the chain has *some* per-warp delay to model around.
SC_MODULE(ShaderCoreCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(ShaderCoreCa);
    explicit ShaderCoreCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
