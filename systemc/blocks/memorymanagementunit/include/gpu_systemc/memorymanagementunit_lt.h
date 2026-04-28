#pragma once
#include <cstdint>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// MMU (TLM-LT). Identity-map placeholder. Enforces a `va_limit`; an
// access whose [addr, addr+size) escapes the cap sets req->fault and
// is forwarded with no L2/MC side effect (downstream blocks honour the
// flag). Mirrors memorymanagementunit_ca.
SC_MODULE(MemoryManagementUnitLt) {
    tlm_utils::simple_target_socket<MemoryManagementUnitLt>     target;
    tlm_utils::simple_initiator_socket<MemoryManagementUnitLt>  initiator;

    uint64_t va_limit = 1ull << 30;     // 1 GiB default cap

    SC_HAS_PROCESS(MemoryManagementUnitLt);
    explicit MemoryManagementUnitLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
