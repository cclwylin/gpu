#pragma once
#include <queue>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// CP block: TLM initiator. Sprint 5: drains a small queue of ShaderJob
// pointers (driver-side enqueue) and forwards each to its bound target via
// b_transport. Real CP will parse a DRAM ring buffer; this is the structural
// stand-in.
SC_MODULE(CommandProcessor) {
    tlm_utils::simple_initiator_socket<CommandProcessor> initiator;

    SC_HAS_PROCESS(CommandProcessor);
    explicit CommandProcessor(sc_core::sc_module_name name);

    // Driver-side enqueue (SystemC thread will pick these up).
    void enqueue(ShaderJob* job);

private:
    std::queue<ShaderJob*> queue_;
    sc_core::sc_event      event_;

    void thread();
};

}  // namespace gpu::systemc
