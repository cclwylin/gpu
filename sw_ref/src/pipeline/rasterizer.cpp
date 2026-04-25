#include "gpu/pipeline.h"

#include <algorithm>
#include <cmath>

namespace gpu::pipeline {

namespace {

inline float edge_fn(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

// D3D rotated-grid 4x sample pattern, per docs/msaa_spec.md §2.
// Offsets in 1/16-pixel units, relative to pixel centre.
struct SampleOffset { float dx, dy; };
constexpr SampleOffset kMsaaPattern[4] = {
    { -2.0f / 16.0f, -6.0f / 16.0f },
    {  6.0f / 16.0f, -2.0f / 16.0f },
    { -6.0f / 16.0f,  2.0f / 16.0f },
    {  2.0f / 16.0f,  6.0f / 16.0f },
};

}  // namespace

void rasterizer(Context& ctx,
                std::span<const Triangle> tris,
                std::vector<Quad>& out_quads) {
    out_quads.clear();
    const int  W = ctx.fb.width;
    const int  H = ctx.fb.height;
    const bool msaa = ctx.fb.msaa_4x;

    for (const auto& tri : tris) {
        const auto& v0 = tri.v[0];
        const auto& v1 = tri.v[1];
        const auto& v2 = tri.v[2];

        const float vx0 = v0.pos[0];
        const float vy0 = v0.pos[1];
        const float vx1 = v1.pos[0];
        const float vy1 = v1.pos[1];
        const float vx2 = v2.pos[0];
        const float vy2 = v2.pos[1];

        const float area = edge_fn(vx0, vy0, vx1, vy1, vx2, vy2);
        if (area == 0.0f) continue;
        const float inv_area = 1.0f / area;

        auto fmin3 = [](float a, float b, float c) { return std::min(a, std::min(b, c)); };
        auto fmax3 = [](float a, float b, float c) { return std::max(a, std::max(b, c)); };
        int xmin_b = std::max(0,     static_cast<int>(std::floor(fmin3(vx0, vx1, vx2))));
        int xmax_b = std::min(W - 1, static_cast<int>(std::ceil (fmax3(vx0, vx1, vx2))));
        int ymin_b = std::max(0,     static_cast<int>(std::floor(fmin3(vy0, vy1, vy2))));
        int ymax_b = std::min(H - 1, static_cast<int>(std::ceil (fmax3(vy0, vy1, vy2))));
        // Scissor (Sprint 17): clip the bbox so we never visit fragments
        // outside the box.
        if (ctx.draw.scissor_enable) {
            const int sx0 = ctx.draw.scissor_x;
            const int sy0 = ctx.draw.scissor_y;
            const int sx1 = ctx.draw.scissor_x + ctx.draw.scissor_w - 1;
            const int sy1 = ctx.draw.scissor_y + ctx.draw.scissor_h - 1;
            xmin_b = std::max(xmin_b, sx0);
            ymin_b = std::max(ymin_b, sy0);
            xmax_b = std::min(xmax_b, sx1);
            ymax_b = std::min(ymax_b, sy1);
        }
        const int xmin = xmin_b, xmax = xmax_b, ymin = ymin_b, ymax = ymax_b;

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
                    // Per-sample edge eval (4× rotated grid).
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

                // Per-pixel-centre barycentric for varying / depth (per-pixel
                // shading even when MSAA is on — sample-shading is out of scope).
                const float cx = px + 0.5f;
                const float cy = py + 0.5f;
                const float w0 = edge_fn(vx1, vy1, vx2, vy2, cx, cy);
                const float w1 = edge_fn(vx2, vy2, vx0, vy0, cx, cy);
                const float w2 = edge_fn(vx0, vy0, vx1, vy1, cx, cy);
                const float l0 = w0 * inv_area;
                const float l1 = w1 * inv_area;
                const float l2 = w2 * inv_area;

                const float inv_w = l0 * v0.pos[3] + l1 * v1.pos[3] + l2 * v2.pos[3];
                const float w     = (inv_w == 0.0f) ? 0.0f : (1.0f / inv_w);
                auto interp4 = [&](const Vec4f& a, const Vec4f& b, const Vec4f& c) {
                    Vec4f r{};
                    for (int k = 0; k < 4; ++k) {
                        r[k] = (l0 * a[k] * v0.pos[3] +
                                l1 * b[k] * v1.pos[3] +
                                l2 * c[k] * v2.pos[3]) * w;
                    }
                    return r;
                };

                Fragment frag{};
                frag.pos = {px, py};
                frag.coverage_mask = mask;
                frag.depth = l0 * v0.pos[2] + l1 * v1.pos[2] + l2 * v2.pos[2];
                frag.varying_count = std::max(v0.varying_count,
                                              std::max(v1.varying_count,
                                                       v2.varying_count));
                for (uint8_t k = 0; k < frag.varying_count; ++k) {
                    frag.varying[k] = interp4(v0.varying[k],
                                              v1.varying[k],
                                              v2.varying[k]);
                }

                Quad q{};
                q.frags[0] = frag;
                q.frags[1].coverage_mask = 0;
                q.frags[2].coverage_mask = 0;
                q.frags[3].coverage_mask = 0;
                out_quads.push_back(q);
            }
        }
    }
}

}  // namespace gpu::pipeline
