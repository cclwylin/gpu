#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// SC block: TLM target. Wraps the existing single-thread ISA simulator.
// Each transaction = one shader job (one warp lane in Sprint 5 terms).
SC_MODULE(ShaderCoreLt) {
    tlm_utils::simple_target_socket<ShaderCoreLt> target;

    SC_HAS_PROCESS(ShaderCoreLt);
    explicit ShaderCoreLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
