#pragma once
#include <cstdint>
#include <systemc>
#include <vector>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 27 — Phase 2 cycle-accurate MemoryController (MC_ca).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (MemRequest*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Owns a backing DRAM model (std::vector<uint8_t>, sized by ctor).
// Per accepted MemRequest:
//   - read:  copies size bytes from dram[addr..addr+size] into req->data
//   - write: copies req->data into dram[addr..addr+size]
//
// Forwards the same pointer downstream so a chain testbench can
// observe each request once it has been serviced. Fault-flagged
// requests are forwarded with no DRAM access.
//
// Timing placeholder: bank latency 12 cycles per request (real DRAM:
// 4-bank × 8-burst × tCK; modeled in Phase 2.x).
SC_MODULE(MemoryControllerCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(MemoryControllerCa);
    explicit MemoryControllerCa(sc_core::sc_module_name name,
                                size_t dram_bytes = 64 * 1024);

    const std::vector<uint8_t>& dram() const { return dram_; }
    std::vector<uint8_t>&       mut_dram()  { return dram_; }

private:
    std::vector<uint8_t> dram_;
    void thread();
};

}  // namespace gpu::systemc
