#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// PFO (TLM-LT). Consumes a PfoJob* and runs gpu::pipeline::per_fragment_ops
// against its (ctx, quad). Mirrors perfragmentops_ca's functional path.
SC_MODULE(PerFragmentOpsLt) {
    tlm_utils::simple_target_socket<PerFragmentOpsLt> target;

    SC_HAS_PROCESS(PerFragmentOpsLt);
    explicit PerFragmentOpsLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
