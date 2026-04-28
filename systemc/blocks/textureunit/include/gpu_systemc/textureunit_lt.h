#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// TMU (TLM-LT). Consumes a TextureJob*: walks requests[i] against
// job->tex via gpu::sample_texture, fills samples[i] in order.
// Mirrors textureunit_ca's functional behaviour without the wire-level
// cycle pump.
SC_MODULE(TextureUnitLt) {
    tlm_utils::simple_target_socket<TextureUnitLt> target;

    SC_HAS_PROCESS(TextureUnitLt);
    explicit TextureUnitLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
