#include "gpu/pipeline.h"

#include <algorithm>
#include <cmath>

namespace gpu::pipeline {

namespace {

// Edge function (positive when point is inside, w.r.t. CCW winding).
inline float edge_fn(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

}  // namespace

// Skeleton rasterizer:
//   - 1× sample (pixel center)
//   - integer pixel iteration over screen bounding box
//   - barycentric varying interpolation (perspective correct via 1/w)
//   - top-left rule using edge >= 0 with tie-break (kept simple here)
//   - emits per-pixel "quads" of size 1 (Phase 1 will batch into 2x2)
void rasterizer(Context& ctx,
                std::span<const Triangle> tris,
                std::vector<Quad>& out_quads) {
    out_quads.clear();
    const int W = ctx.fb.width;
    const int H = ctx.fb.height;

    for (const auto& tri : tris) {
        const auto& v0 = tri.v[0];
        const auto& v1 = tri.v[1];
        const auto& v2 = tri.v[2];

        // NB: locals deliberately not named y0/y1 — those clash with the
        // Bessel function symbols pulled in by <cmath> on libstdc++.
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
        const int xmin = std::max(0,     static_cast<int>(std::floor(fmin3(vx0, vx1, vx2))));
        const int xmax = std::min(W - 1, static_cast<int>(std::ceil (fmax3(vx0, vx1, vx2))));
        const int ymin = std::max(0,     static_cast<int>(std::floor(fmin3(vy0, vy1, vy2))));
        const int ymax = std::min(H - 1, static_cast<int>(std::ceil (fmax3(vy0, vy1, vy2))));

        for (int py = ymin; py <= ymax; ++py) {
            for (int px = xmin; px <= xmax; ++px) {
                const float cx = px + 0.5f;
                const float cy = py + 0.5f;

                const float w0 = edge_fn(vx1, vy1, vx2, vy2, cx, cy);
                const float w1 = edge_fn(vx2, vy2, vx0, vy0, cx, cy);
                const float w2 = edge_fn(vx0, vy0, vx1, vy1, cx, cy);

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

                const float l0 = w0 * inv_area;
                const float l1 = w1 * inv_area;
                const float l2 = w2 * inv_area;

                // Perspective-correct varying interpolation:
                // pos[3] holds 1/w from PA stage.
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
                frag.coverage_mask = 0x1;
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
                q.frags[0] = frag;     // skeleton: single-fragment quad
                q.frags[1].coverage_mask = 0;
                q.frags[2].coverage_mask = 0;
                q.frags[3].coverage_mask = 0;
                out_quads.push_back(q);
            }
        }
    }
}

}  // namespace gpu::pipeline
