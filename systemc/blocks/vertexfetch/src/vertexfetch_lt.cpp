#include "gpu_systemc/vertexfetch_lt.h"

namespace gpu::systemc {

VertexFetchLt::VertexFetchLt(sc_core::sc_module_name name)
    : sc_module(name), target("target"), initiator("initiator") {
    target.register_b_transport(this, &VertexFetchLt::b_transport);
}

void VertexFetchLt::b_transport(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<VertexFetchJob*>(trans.get_data_ptr());
    if (!job || !job->vs_code) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    job->vs_outputs.assign(job->vertex_count, {});

    for (int v = 0; v < job->vertex_count; ++v) {
        ShaderJob sj{};
        sj.code = job->vs_code;
        sj.constants = job->constants;
        sj.attr_count = job->attr_count;
        sj.is_vs = true;
        // First 8 attribute slots get loaded; vertex layout is per-attrib vec4.
        for (int s = 0; s < job->attr_count && s < 8; ++s) {
            sj.attrs[s] = job->vertices[v][s];
        }

        tlm::tlm_generic_payload sub;
        sc_core::sc_time         sd = sc_core::SC_ZERO_TIME;
        sub.set_command(tlm::TLM_WRITE_COMMAND);
        sub.set_data_ptr(reinterpret_cast<unsigned char*>(&sj));
        sub.set_data_length(sizeof(sj));
        sub.set_streaming_width(sizeof(sj));
        sub.set_byte_enable_ptr(nullptr);
        sub.set_dmi_allowed(false);
        sub.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        initiator->b_transport(sub, sd);
        delay += sd;
        if (sub.get_response_status() != tlm::TLM_OK_RESPONSE) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        job->vs_outputs[v] = sj.outputs;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
