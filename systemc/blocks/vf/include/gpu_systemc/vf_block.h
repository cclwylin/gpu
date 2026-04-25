#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// VF: TLM target (driven by CP with VertexFetchJob*).
//     For each vertex, builds a ShaderJob and forwards via initiator to SC
//     (which runs the VS), then collects per-vertex outputs.
SC_MODULE(VertexFetch) {
    tlm_utils::simple_target_socket<VertexFetch>     target;     // from CP
    tlm_utils::simple_initiator_socket<VertexFetch>  initiator;  // -> SC

    SC_HAS_PROCESS(VertexFetch);
    explicit VertexFetch(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
