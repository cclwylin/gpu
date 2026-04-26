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
            // Sutherland-Hodgman near/far Z clip on each triangle.
            using Vert = std::array<gpu::sim::Vec4, 4>;
            auto lerp_v = [](const Vert& a, const Vert& b, float t) {
                Vert r{};
                for (int v = 0; v < 4; ++v)
                    for (int k = 0; k < 4; ++k)
                        r[v][k] = a[v][k] + t * (b[v][k] - a[v][k]);
                return r;
            };
            auto clip_against = [&](std::vector<Vert>& in,
                                    std::vector<Vert>& out,
                                    auto dist) {
                out.clear();
                if (in.empty()) return;
                for (size_t k = 0; k < in.size(); ++k) {
                    const Vert& a = in[k];
                    const Vert& b = in[(k + 1) % in.size()];
                    const float da = dist(a[0]);
                    const float db = dist(b[0]);
                    if (da >= 0.0f) out.push_back(a);
                    if ((da >= 0.0f) != (db >= 0.0f)) {
                        const float t = da / (da - db);
                        out.push_back(lerp_v(a, b, t));
                    }
                }
            };
            for (size_t i = 0; i + 2 < vsout.size(); i += 3) {
                std::vector<Vert> p, q;
                p.reserve(5); q.reserve(5);
                Vert va{}, vb{}, vc{};
                va[0] = vsout[i][0];     va[1] = vsout[i][1];
                va[2] = vsout[i][2];     va[3] = vsout[i][3];
                vb[0] = vsout[i+1][0];   vb[1] = vsout[i+1][1];
                vb[2] = vsout[i+1][2];   vb[3] = vsout[i+1][3];
                vc[0] = vsout[i+2][0];   vc[1] = vsout[i+2][1];
                vc[2] = vsout[i+2][2];   vc[3] = vsout[i+2][3];
                p = {va, vb, vc};
                clip_against(p, q,
                    [](const gpu::sim::Vec4& v) { return v[2] + v[3]; }); // near
                if (q.empty()) continue;
                clip_against(q, p,
                    [](const gpu::sim::Vec4& v) { return v[3] - v[2]; }); // far
                if (p.size() < 3) continue;
                // Fan-triangulate the clipped polygon (3..5 verts).
                for (size_t fi = 1; fi + 1 < p.size(); ++fi) {
                    std::array<gpu::sim::Vec4, 4> a0 = p[0], a1 = p[fi], a2 = p[fi+1];
                    std::array<std::array<gpu::sim::Vec4, 4>, 3> tri;
                    tri[0] = a0; tri[1] = a1; tri[2] = a2;
                    tri[0][0] = persp_divide_and_viewport(a0[0],
                        job->vp_x, job->vp_y, job->vp_w, job->vp_h);
                    tri[1][0] = persp_divide_and_viewport(a1[0],
                        job->vp_x, job->vp_y, job->vp_w, job->vp_h);
                    tri[2][0] = persp_divide_and_viewport(a2[0],
                        job->vp_x, job->vp_y, job->vp_w, job->vp_h);
                    if (job->cull_back &&
                        back_face_cull(tri[0][0], tri[1][0], tri[2][0])) continue;
                    job->triangles.push_back(tri);
                    wait();
                    wait();
                }
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
