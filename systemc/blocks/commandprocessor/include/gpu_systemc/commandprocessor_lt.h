#pragma once
#include <queue>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// CP block: TLM initiator. Pulls jobs from a host-driven queue and forwards
// each as a b_transport. Sprint 10: payload is opaque (void*) so CP can
// front any downstream block (was ShaderJob* in Sprint 5; now VertexFetchJob*
// once CP is bound to VF, but CP is agnostic).
SC_MODULE(CommandProcessorLt) {
    tlm_utils::simple_initiator_socket<CommandProcessorLt> initiator;

    SC_HAS_PROCESS(CommandProcessorLt);
    explicit CommandProcessorLt(sc_core::sc_module_name name);

    void enqueue(void* job);

private:
    std::queue<void*> queue_;
    sc_core::sc_event event_;

    void thread();
};

}  // namespace gpu::systemc
