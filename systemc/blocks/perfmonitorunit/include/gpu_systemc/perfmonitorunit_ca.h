#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 28 — Phase 2 cycle-accurate Performance Monitor (PMU_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (PmuJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Single rolling cycle counter, incremented every clock edge from
// reset deassertion. Per accepted PmuJob: writes the current count
// into job->cycles and forwards the same pointer downstream.
//
// Real PMU has a counter bank (cycles, instructions retired, cache
// hits/misses, …); modelled in Phase 2.x.
SC_MODULE(PerfMonitorUnitCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(PerfMonitorUnitCa);
    explicit PerfMonitorUnitCa(sc_core::sc_module_name name);

private:
    void counter_thread();
    void access_thread();
    sc_core::sc_signal<sc_dt::uint64> cycles_;
};

}  // namespace gpu::systemc
