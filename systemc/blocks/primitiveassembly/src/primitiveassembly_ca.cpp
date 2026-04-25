#include "gpu_systemc/primitiveassembly_ca.h"

namespace gpu::systemc {

namespace {

// Mirrors primitiveassembly_lt.cpp's helpers; kept local for clarity until
// LT/CA share a single math module.
gpu::sim::Vec4 persp_divide_and_viewport(const gpu::sim::Vec4& clip,
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

PrimitiveAssemblyCa::PrimitiveAssemblyCa(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      job_valid_i("job_valid_i"),
      job_ready_o("job_ready_o"),
      job_data_i ("job_data_i"),
      tri_valid_o("tri_valid_o"),
      tri_ready_i("tri_ready_i"),
      tri_data_o ("tri_data_o") {
    SC_CTHREAD(thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void PrimitiveAssemblyCa::thread() {
    job_ready_o.write(false);
    tri_valid_o.write(false);
    tri_data_o.write(0);

    while (true) {
        // ---- accept upstream job ----
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        PrimAssemblyJob* const job =
            reinterpret_cast<PrimAssemblyJob*>(job_word);
        job_ready_o.write(false);

        // ---- run PA math in place ----
        if (job) {
            job->triangles.clear();
            const auto& vsout = job->vs_outputs;
            for (size_t i = 0; i + 2 < vsout.size(); i += 3) {
                std::array<std::array<gpu::sim::Vec4, 4>, 3> tri;
                tri[0] = vsout[i];
                tri[1] = vsout[i + 1];
                tri[2] = vsout[i + 2];
                tri[0][0] = persp_divide_and_viewport(vsout[i][0],
                                                     job->vp_x, job->vp_y,
                                                     job->vp_w, job->vp_h);
                tri[1][0] = persp_divide_and_viewport(vsout[i + 1][0],
                                                     job->vp_x, job->vp_y,
                                                     job->vp_w, job->vp_h);
                tri[2][0] = persp_divide_and_viewport(vsout[i + 2][0],
                                                     job->vp_x, job->vp_y,
                                                     job->vp_w, job->vp_h);
                if (job->cull_back &&
                    back_face_cull(tri[0][0], tri[1][0], tri[2][0])) continue;
                job->triangles.push_back(tri);
                wait();        // 1 cycle / triangle (post-divide)
                wait();        // + 1 cycle / triangle (cull / emit)
            }
        }

        // ---- forward downstream ----
        tri_valid_o.write(true);
        tri_data_o.write(job_word);
        wait();
        while (!tri_ready_i.read()) wait();
        tri_valid_o.write(false);
        tri_data_o.write(0);
        wait();
    }
}

}  // namespace gpu::systemc
