#pragma once
#include <queue>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// CP block: TLM initiator. Pulls jobs from a host-driven queue and
// forwards each to the correct downstream block.
//
// Stage tag — host code calls enqueue(Stage::PA, &job) and CP routes
// through the matching initiator socket. The single-arg enqueue stays
// for backwards compat (defaults to Stage::VF, the original chain
// front-end). Sockets that have no bound target are simply skipped at
// routing time, so a unit test can use just the one socket it cares
// about.
SC_MODULE(CommandProcessorLt) {
    enum class Stage { VF, PA, RS, TMU, PFO };

    tlm_utils::simple_initiator_socket<CommandProcessorLt> initiator;       // VF (legacy alias)
    tlm_utils::simple_initiator_socket<CommandProcessorLt> pa_initiator;
    tlm_utils::simple_initiator_socket<CommandProcessorLt> rs_initiator;
    tlm_utils::simple_initiator_socket<CommandProcessorLt> tmu_initiator;
    tlm_utils::simple_initiator_socket<CommandProcessorLt> pfo_initiator;

    SC_HAS_PROCESS(CommandProcessorLt);
    explicit CommandProcessorLt(sc_core::sc_module_name name);

    void enqueue(void* job);                        // Stage::VF (default)
    void enqueue(Stage stage, void* job);

private:
    struct Entry { Stage stage; void* job; };
    std::queue<Entry> queue_;
    sc_core::sc_event event_;

    void thread();
};

}  // namespace gpu::systemc
