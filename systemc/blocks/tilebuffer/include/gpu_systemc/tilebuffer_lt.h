#pragma once
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// TBF (TLM-LT). Storage placeholder: the on-chip sample SRAM lives on
// the host as Context::fb.{color,depth,stencil}_samples. TBF_lt accepts
// a TileFlushJob*, charges no work (matches tilebuffer_ca's zero-cycle
// flush), and forwards via initiator to RSV.
SC_MODULE(TileBufferLt) {
    tlm_utils::simple_target_socket<TileBufferLt>     target;     // from PFO
    tlm_utils::simple_initiator_socket<TileBufferLt>  initiator;  // -> RSV

    SC_HAS_PROCESS(TileBufferLt);
    explicit TileBufferLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
