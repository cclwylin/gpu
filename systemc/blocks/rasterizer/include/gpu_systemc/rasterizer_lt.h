#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// RS: TLM target. Consumes a RasterJob (screen-space triangle list, viewport
// already applied). For each pixel covered by a triangle, emits a
// RasterFragment with coverage mask + per-pixel-centre interpolated
// varying. Per-pixel shading; sample-shading deliberately out of scope.
SC_MODULE(RasterizerLt) {
    tlm_utils::simple_target_socket<RasterizerLt> target;

    SC_HAS_PROCESS(RasterizerLt);
    explicit RasterizerLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
