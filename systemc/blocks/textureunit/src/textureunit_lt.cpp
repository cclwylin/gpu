#include "gpu_systemc/textureunit_lt.h"

#include "gpu/texture.h"

namespace gpu::systemc {

TextureUnitLt::TextureUnitLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &TextureUnitLt::b_transport);
}

void TextureUnitLt::b_transport(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<TextureJob*>(trans.get_data_ptr());
    if (!job || !job->tex) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    job->samples.resize(job->requests.size());
    for (size_t i = 0; i < job->requests.size(); ++i) {
        job->samples[i] = gpu::sample_texture(
            *job->tex, job->requests[i].u, job->requests[i].v);
    }
    delay += sc_core::sc_time(static_cast<double>(job->requests.size()),
                              sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
