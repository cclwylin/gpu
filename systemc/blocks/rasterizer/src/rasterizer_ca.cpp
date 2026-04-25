#include "gpu_systemc/rasterizer_ca.h"

#include <algorithm>
#include <cmath>

namespace gpu::systemc {
namespace {

// Mirrors rasterizer_lt.cpp helpers; kept local until LT/CA share a math
// module across blocks.
struct SampleOffset { float dx, dy; };
constexpr SampleOffset kMsaaPattern[4] = {
    { -2.0f / 16.0f, -6.0f / 16.0f },
    {  6.0f / 16.0f, -2.0f / 16.0f },
    { -6.0f / 16.0f,  2.0f / 16.0f },
    {  2.0f / 16.0f,  6.0f / 16.0f },
};

inline float edge_fn(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void rasterize(RasterJob& job) {
    job.fragments.clear();
    const int  W = job.fb_w;
    const int  H = job.fb_h;
    const bool msaa = job.msaa_4x;

    for (const auto& tri : job.triangles) {
        const float vx0 = tri[0][0][0], vy0 = tri[0][0][1];
        const float vx1 = tri[1][0][0], vy1 = tri[1][0][1];
        const float vx2 = tri[2][0][0], vy2 = tri[2][0][1];

        const float area = edge_fn(vx0, vy0, vx1, vy1, vx2, vy2);
        if (area == 0.0f) continue;
        const float inv_area = 1.0f / area;

        auto fmin3 = [](float a, float b, float c) { return std::min(a, std::min(b, c)); };
        auto fmax3 = [](float a, float b, float c) { return std::max(a, std::max(b, c)); };
        const int xmin = std::max(0,     static_cast<int>(std::floor(fmin3(vx0, vx1, vx2))));
        const int xmax = std::min(W - 1, static_cast<int>(std::ceil (fmax3(vx0, vx1, vx2))));
        const int ymin = std::max(0,     static_cast<int>(std::floor(fmin3(vy0, vy1, vy2))));
        const int ymax = std::min(H - 1, static_cast<int>(std::ceil (fmax3(vy0, vy1, vy2))));

        for (int py = ymin; py <= ymax; ++py) {
            for (int px = xmin; px <= xmax; ++px) {
                uint8_t mask = 0;
                if (!msaa) {
                    const float cx = px + 0.5f, cy = py + 0.5f;
                    const float w0 = edge_fn(vx1, vy1, vx2, vy2, cx, cy);
                    const float w1 = edge_fn(vx2, vy2, vx0, vy0, cx, cy);
                    const float w2 = edge_fn(vx0, vy0, vx1, vy1, cx, cy);
                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) mask = 0x1;
                } else {
                    for (int s = 0; s < 4; ++s) {
                        const float cx = px + 0.5f + kMsaaPattern[s].dx;
                        const float cy = py + 0.5f + kMsaaPattern[s].dy;
                        const float w0 = edge_fn(vx1, vy1, vx2, vy2, cx, cy);
                        const float w1 = edge_fn(vx2, vy2, vx0, vy0, cx, cy);
                        const float w2 = edge_fn(vx0, vy0, vx1, vy1, cx, cy);
                        if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                            mask |= static_cast<uint8_t>(1 << s);
                    }
                }
                if (mask == 0) continue;

                const float cx = px + 0.5f, cy = py + 0.5f;
                const float w0 = edge_fn(vx1, vy1, vx2, vy2, cx, cy);
                const float w1 = edge_fn(vx2, vy2, vx0, vy0, cx, cy);
                const float w2 = edge_fn(vx0, vy0, vx1, vy1, cx, cy);
                const float l0 = w0 * inv_area;
                const float l1 = w1 * inv_area;
                const float l2 = w2 * inv_area;

                const float invw_a = tri[0][0][3];
                const float invw_b = tri[1][0][3];
                const float invw_c = tri[2][0][3];
                const float inv_w = l0 * invw_a + l1 * invw_b + l2 * invw_c;
                const float w = (inv_w == 0.0f) ? 0.0f : 1.0f / inv_w;

                RasterFragment f{};
                f.x = px; f.y = py;
                f.coverage_mask = mask;
                f.depth = l0 * tri[0][0][2] + l1 * tri[1][0][2] + l2 * tri[2][0][2];
                for (int k = 0; k < job.varying_count && k < 4; ++k) {
                    for (int c = 0; c < 4; ++c) {
                        f.varying[k][c] = (l0 * tri[0][k + 1][c] * invw_a +
                                           l1 * tri[1][k + 1][c] * invw_b +
                                           l2 * tri[2][k + 1][c] * invw_c) * w;
                    }
                }
                job.fragments.push_back(f);
            }
        }
    }
}

}  // namespace

RasterizerCa::RasterizerCa(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      job_valid_i ("job_valid_i"),
      job_ready_o ("job_ready_o"),
      job_data_i  ("job_data_i"),
      frag_valid_o("frag_valid_o"),
      frag_ready_i("frag_ready_i"),
      frag_data_o ("frag_data_o") {
    SC_CTHREAD(thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void RasterizerCa::thread() {
    job_ready_o.write(false);
    frag_valid_o.write(false);
    frag_data_o.write(0);

    while (true) {
        // ---- accept upstream job ----
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        RasterJob* const job = reinterpret_cast<RasterJob*>(job_word);
        job_ready_o.write(false);

        // ---- rasterise in place ----
        if (job) {
            rasterize(*job);
            // Timing placeholder: 1 cycle per fragment.
            for (size_t k = 0; k < job->fragments.size(); ++k) wait();
        }

        // ---- forward downstream ----
        frag_valid_o.write(true);
        frag_data_o.write(job_word);
        wait();
        while (!frag_ready_i.read()) wait();
        frag_valid_o.write(false);
        frag_data_o.write(0);
        wait();
    }
}

}  // namespace gpu::systemc
