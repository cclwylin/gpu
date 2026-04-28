#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// RSV (TLM-LT). Consumes a TileFlushJob* and runs gpu::pipeline::resolve
// against its ctx (4× MSAA box-filter → 1× RGBA8 in fb.color, no-op when
// MSAA is off). Mirrors resolveunit_ca.
SC_MODULE(ResolveUnitLt) {
    tlm_utils::simple_target_socket<ResolveUnitLt> target;

    SC_HAS_PROCESS(ResolveUnitLt);
    explicit ResolveUnitLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
