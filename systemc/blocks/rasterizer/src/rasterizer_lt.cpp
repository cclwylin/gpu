#include "gpu_systemc/rasterizer_lt.h"

#include <algorithm>
#include <cmath>

namespace gpu::systemc {
namespace {

// D3D rotated-grid 4× sample pattern (matches sw_ref/src/pipeline/rasterizer.cpp).
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

}  // namespace

RasterizerLt::RasterizerLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &RasterizerLt::b_transport);
}

void RasterizerLt::b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay) {
    auto* job = reinterpret_cast<RasterJob*>(trans.get_data_ptr());
    if (!job) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    job->fragments.clear();
    const int W = job->fb_w;
    const int H = job->fb_h;
    const bool msaa = job->msaa_4x;

    for (const auto& tri : job->triangles) {
        // Vertex screen-space xy + depth + 1/w sit in slot 0 (pos).
        const float vx0 = tri[0][0][0], vy0 = tri[0][0][1];
        const float vx1 = tri[1][0][0], vy1 = tri[1][0][1];
        const float vx2 = tri[2][0][0], vy2 = tri[2][0][1];

        const float area = edge_fn(vx0, vy0, vx1, vy1, vx2, vy2);
        if (area == 0.0f) continue;
        const float inv_area = 1.0f / area;

        const auto fmin3 = [](float a, float b, float c) { return std::min(a, std::min(b, c)); };
        const auto fmax3 = [](float a, float b, float c) { return std::max(a, std::max(b, c)); };
        const int xmin = std::max(0,     static_cast<int>(std::floor(fmin3(vx0, vx1, vx2))));
        const int xmax = std::min(W - 1, static_cast<int>(std::ceil (fmax3(vx0, vx1, vx2))));
        const int ymin = std::max(0,     static_cast<int>(std::floor(fmin3(vy0, vy1, vy2))));
        const int ymax = std::min(H - 1, static_cast<int>(std::ceil (fmax3(vy0, vy1, vy2))));

        for (int py = ymin; py <= ymax; ++py) {
            for (int px = xmin; px <= xmax; ++px) {
                uint8_t mask = 0;
                if (!msaa) {
                    const float cx = px + 0.5f;
                    const float cy = py + 0.5f;
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
                        if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                            mask |= static_cast<uint8_t>(1 << s);
                        }
                    }
                }
                if (mask == 0) continue;

                // Per-pixel-centre barycentric for varying / depth.
                const float cx = px + 0.5f, cy = py + 0.5f;
                const float w0 = edge_fn(vx1, vy1, vx2, vy2, cx, cy);
                const float w1 = edge_fn(vx2, vy2, vx0, vy0, cx, cy);
                const float w2 = edge_fn(vx0, vy0, vx1, vy1, cx, cy);
                const float l0 = w0 * inv_area;
                const float l1 = w1 * inv_area;
                const float l2 = w2 * inv_area;

                // pos[3] holds 1/w from PA.
                const float invw_a = tri[0][0][3];
                const float invw_b = tri[1][0][3];
                const float invw_c = tri[2][0][3];
                const float inv_w  = l0 * invw_a + l1 * invw_b + l2 * invw_c;
                const float w      = (inv_w == 0.0f) ? 0.0f : 1.0f / inv_w;

                RasterFragment f{};
                f.x = px; f.y = py;
                f.coverage_mask = mask;
                f.depth = l0 * tri[0][0][2] + l1 * tri[1][0][2] + l2 * tri[2][0][2];
                for (int k = 0; k < job->varying_count && k < 4; ++k) {
                    for (int c = 0; c < 4; ++c) {
                        f.varying[k][c] = (l0 * tri[0][k + 1][c] * invw_a +
                                           l1 * tri[1][k + 1][c] * invw_b +
                                           l2 * tri[2][k + 1][c] * invw_c) * w;
                    }
                }
                job->fragments.push_back(f);
            }
        }
    }

    // Cycle model placeholder: 1 ns per emitted fragment.
    delay += sc_core::sc_time(static_cast<double>(job->fragments.size()),
                              sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
