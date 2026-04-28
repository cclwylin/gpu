#include "gpu_systemc/shadercore_lt.h"

#include <cstring>

namespace gpu::systemc {

ShaderCoreLt::ShaderCoreLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &ShaderCoreLt::b_transport);
}

void ShaderCoreLt::b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay) {
    // The job pointer is carried via the payload's data_ptr. This is a
    // Sprint-5 shortcut; real impl will use TLM extensions.
    auto* job = reinterpret_cast<ShaderJob*>(trans.get_data_ptr());
    if (!job || !job->code) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    gpu::sim::ThreadState t{};
    t.c = job->constants;
    if (job->is_vs) {
        for (int i = 0; i < job->attr_count && i < 8; ++i) {
            t.r[i] = job->attrs[i];
        }
    } else {
        for (int i = 0; i < job->varying_in_count && i < 8; ++i) {
            t.varying[i] = job->varying_in[i];
        }
    }
    auto er = gpu::sim::execute(*job->code, t);
    if (!er.ok) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    job->outputs = t.o;
    job->lane_active = t.lane_active;

    // Sprint 5: model 1 ns per executed instruction (placeholder).
    delay += sc_core::sc_time(static_cast<double>(job->code->size()),
                              sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
