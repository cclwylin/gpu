#include "gpu_systemc/primitiveassembly_lt.h"

namespace gpu::systemc {

namespace {

gpu::sim::Vec4 perspective_divide_and_viewport(const gpu::sim::Vec4& clip,
                                               int vp_x, int vp_y,
                                               int vp_w, int vp_h) {
    gpu::sim::Vec4 out = clip;
    const float w = clip[3];
    if (w == 0.0f) return out;
    const float inv_w = 1.0f / w;
    const float ndc_x = clip[0] * inv_w;
    const float ndc_y = clip[1] * inv_w;
    const float ndc_z = clip[2] * inv_w;
    out[0] = (ndc_x * 0.5f + 0.5f) * vp_w + vp_x;
    out[1] = (ndc_y * 0.5f + 0.5f) * vp_h + vp_y;
    out[2] = ndc_z * 0.5f + 0.5f;
    out[3] = inv_w;
    return out;
}

bool back_face_cull(const gpu::sim::Vec4& a,
                    const gpu::sim::Vec4& b,
                    const gpu::sim::Vec4& c) {
    const float ax = a[0], ay = a[1];
    const float bx = b[0], by = b[1];
    const float cx = c[0], cy = c[1];
    const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    return area <= 0.0f;
}

}  // namespace

PrimitiveAssemblyLt::PrimitiveAssemblyLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &PrimitiveAssemblyLt::b_transport);
}

void PrimitiveAssemblyLt::b_transport(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<PrimAssemblyJob*>(trans.get_data_ptr());
    if (!job) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    job->triangles.clear();
    const auto& vsout = job->vs_outputs;
    if (vsout.size() < 3) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    auto emit = [&](const std::array<gpu::sim::Vec4, 8>& a,
                    const std::array<gpu::sim::Vec4, 8>& b,
                    const std::array<gpu::sim::Vec4, 8>& c) {
        std::array<std::array<gpu::sim::Vec4, 8>, 3> tri;
        tri[0] = a; tri[1] = b; tri[2] = c;
        // Transform o0 (clip pos) only; varyings (o1..o3) are pass-through.
        tri[0][0] = perspective_divide_and_viewport(a[0], job->vp_x, job->vp_y,
                                                    job->vp_w, job->vp_h);
        tri[1][0] = perspective_divide_and_viewport(b[0], job->vp_x, job->vp_y,
                                                    job->vp_w, job->vp_h);
        tri[2][0] = perspective_divide_and_viewport(c[0], job->vp_x, job->vp_y,
                                                    job->vp_w, job->vp_h);
        if (job->cull_back && back_face_cull(tri[0][0], tri[1][0], tri[2][0])) {
            return;
        }
        job->triangles.push_back(tri);
    };

    // TRIANGLES mode only.
    for (size_t i = 0; i + 2 < vsout.size(); i += 3) {
        emit(vsout[i], vsout[i + 1], vsout[i + 2]);
    }

    delay += sc_core::sc_time(static_cast<double>(vsout.size()),
                              sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
