#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// L2 (TLM-LT). Pass-through with a 4-cycle hit-latency stamp; no
// actual caching. Mirrors l2cache_ca.
SC_MODULE(L2CacheLt) {
    tlm_utils::simple_target_socket<L2CacheLt>     target;
    tlm_utils::simple_initiator_socket<L2CacheLt>  initiator;

    SC_HAS_PROCESS(L2CacheLt);
    explicit L2CacheLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
