#include "gpu_systemc/perfragmentops_lt.h"

#include "gpu/pipeline.h"
#include "gpu/state.h"

namespace gpu::systemc {

PerFragmentOpsLt::PerFragmentOpsLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &PerFragmentOpsLt::b_transport);
}

void PerFragmentOpsLt::b_transport(tlm::tlm_generic_payload& trans,
                                   sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<PfoJob*>(trans.get_data_ptr());
    if (!job || !job->ctx || !job->quad) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    gpu::pipeline::per_fragment_ops(*job->ctx, *job->quad);
    delay += sc_core::sc_time(4.0, sc_core::SC_NS);  // 4-frag quad placeholder
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
