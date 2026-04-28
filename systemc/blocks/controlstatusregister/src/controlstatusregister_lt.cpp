#include "gpu_systemc/controlstatusregister_lt.h"

namespace gpu::systemc {

ControlStatusRegisterLt::ControlStatusRegisterLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &ControlStatusRegisterLt::b_transport);
    regs_.fill(0);
}

void ControlStatusRegisterLt::b_transport(tlm::tlm_generic_payload& trans,
                                          sc_core::sc_time& delay) {
    auto* req = reinterpret_cast<CsrJob*>(trans.get_data_ptr());
    if (!req || req->reg_idx >= static_cast<uint32_t>(kNumRegs)) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (req->is_write) {
        regs_[req->reg_idx] = req->value;
    } else {
        req->value = regs_[req->reg_idx];
    }
    delay += sc_core::sc_time(1.0, sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
