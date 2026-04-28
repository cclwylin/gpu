#include "gpu_systemc/memorymanagementunit_lt.h"

namespace gpu::systemc {

MemoryManagementUnitLt::MemoryManagementUnitLt(sc_core::sc_module_name name)
    : sc_module(name), target("target"), initiator("initiator") {
    target.register_b_transport(this, &MemoryManagementUnitLt::b_transport);
}

void MemoryManagementUnitLt::b_transport(tlm::tlm_generic_payload& trans,
                                         sc_core::sc_time& delay) {
    auto* req = reinterpret_cast<MemRequest*>(trans.get_data_ptr());
    if (!req) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (req->addr + req->size > va_limit) {
        req->fault = true;
    }
    delay += sc_core::sc_time(1.0, sc_core::SC_NS);

    if (initiator.size() > 0) {
        tlm::tlm_generic_payload sub;
        sc_core::sc_time         sd = sc_core::SC_ZERO_TIME;
        sub.set_command(tlm::TLM_WRITE_COMMAND);
        sub.set_data_ptr(reinterpret_cast<unsigned char*>(req));
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
