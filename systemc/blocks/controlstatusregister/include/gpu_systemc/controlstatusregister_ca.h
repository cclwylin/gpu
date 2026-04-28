#pragma once
#include <array>
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 28 — Phase 2 cycle-accurate CSR (Control / Status Register file).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (CsrJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (same ptr)
//
// Holds 16 × 32-bit host-visible registers. Per accepted CsrJob:
//   - is_write=true  : regs_[reg_idx] = job->value
//   - is_write=false : job->value = regs_[reg_idx]
//
// Real CSR sits behind APB / AHB on the host side; this CA model is
// the chip-internal handshake, not the bus protocol.
SC_MODULE(ControlStatusRegisterCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    SC_HAS_PROCESS(ControlStatusRegisterCa);
    explicit ControlStatusRegisterCa(sc_core::sc_module_name name);

    static constexpr int kNumRegs = 16;

private:
    std::array<uint32_t, kNumRegs> regs_{};
    void thread();
};

}  // namespace gpu::systemc
