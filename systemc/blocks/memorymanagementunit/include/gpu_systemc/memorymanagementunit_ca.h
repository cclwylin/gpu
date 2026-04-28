#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 27 — Phase 2 cycle-accurate MMU.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (MemRequest*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Identity-map placeholder. Real MMU has 32-entry TLB, page-walker
// fallback, ASID; addr translation is pass-through here. A 64-entry
// `va_limit` is enforced — addr ≥ limit sets `fault=true` and the
// request is forwarded with no L2/MC side-effect (downstream blocks
// honour the fault flag).
//
// Timing placeholder: 1 cycle per request.
SC_MODULE(MemoryManagementUnitCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    uint64_t va_limit = 1ull << 30;     // 1 GiB default cap

    SC_HAS_PROCESS(MemoryManagementUnitCa);
    explicit MemoryManagementUnitCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
