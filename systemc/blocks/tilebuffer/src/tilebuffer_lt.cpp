#include "gpu_systemc/tilebuffer_lt.h"

namespace gpu::systemc {

TileBufferLt::TileBufferLt(sc_core::sc_module_name name)
    : sc_module(name), target("target"), initiator("initiator") {
    target.register_b_transport(this, &TileBufferLt::b_transport);
}

void TileBufferLt::b_transport(tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<TileFlushJob*>(trans.get_data_ptr());
    if (!job) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // Forward unchanged to RSV (downstream is optional — only invoke
    // initiator if it is bound, so a unit test can use TBF_lt alone).
    if (initiator.size() > 0) {
        tlm::tlm_generic_payload sub;
        sc_core::sc_time         sd = sc_core::SC_ZERO_TIME;
        sub.set_command(tlm::TLM_WRITE_COMMAND);
        sub.set_data_ptr(reinterpret_cast<unsigned char*>(job));
        sub.set_data_length(0);
        sub.set_streaming_width(0);
        sub.set_byte_enable_ptr(nullptr);
        sub.set_dmi_allowed(false);
        sub.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator->b_transport(sub, sd);
        delay += sd;
        if (sub.get_response_status() != tlm::TLM_OK_RESPONSE) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
