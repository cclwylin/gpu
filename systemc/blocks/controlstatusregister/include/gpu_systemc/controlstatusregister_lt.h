#pragma once
#include <array>
#include <cstdint>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// CSR (TLM-LT). 16 × 32-bit register file. Per accepted CsrJob:
//   - is_write=true  : regs_[reg_idx] = job->value
//   - is_write=false : job->value = regs_[reg_idx]
// Mirrors controlstatusregister_ca; the host-side bus protocol
// (APB / AHB) is out of scope, this is the chip-internal handshake.
SC_MODULE(ControlStatusRegisterLt) {
    tlm_utils::simple_target_socket<ControlStatusRegisterLt> target;

    static constexpr int kNumRegs = 16;

    SC_HAS_PROCESS(ControlStatusRegisterLt);
    explicit ControlStatusRegisterLt(sc_core::sc_module_name name);

private:
    std::array<uint32_t, kNumRegs> regs_{};
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
