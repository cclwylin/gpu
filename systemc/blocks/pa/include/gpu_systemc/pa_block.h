#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// PA: TLM target. Consumes VS-output vertex list, runs perspective divide +
// viewport + back-face cull, emits triangle list (TRIANGLES mode only in
// Sprint 10).
SC_MODULE(PrimitiveAssembly) {
    tlm_utils::simple_target_socket<PrimitiveAssembly> target;

    SC_HAS_PROCESS(PrimitiveAssembly);
    explicit PrimitiveAssembly(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
