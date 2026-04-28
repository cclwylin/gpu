#include "gpu_systemc/resolveunit_lt.h"

#include "gpu/pipeline.h"
#include "gpu/state.h"

namespace gpu::systemc {

ResolveUnitLt::ResolveUnitLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &ResolveUnitLt::b_transport);
}

void ResolveUnitLt::b_transport(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<TileFlushJob*>(trans.get_data_ptr());
    if (!job || !job->ctx) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    gpu::pipeline::resolve(*job->ctx);
    if (job->ctx->fb.msaa_4x) {
        const int n = job->ctx->fb.width * job->ctx->fb.height;
        delay += sc_core::sc_time(static_cast<double>(n), sc_core::SC_NS);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
