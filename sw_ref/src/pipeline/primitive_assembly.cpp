#include "gpu/pipeline.h"

#include <array>
#include <vector>

namespace gpu::pipeline {

namespace {

// Sprint 44 — Sutherland-Hodgman polygon clipping in homogeneous clip
// space. Clips a convex polygon (≤7 verts after near+far) against one
// plane defined by a signed-distance function `dist(v.pos)`. inside =
// dist >= 0. Linearly interpolates `pos` and all `varying` slots
// across the clipped edge.
using PolyBuf = std::vector<Vertex>;

Vertex lerp_vertex(const Vertex& a, const Vertex& b, float t) {
    Vertex r = a;
    for (int k = 0; k < 4; ++k) r.pos[k] = a.pos[k] + t * (b.pos[k] - a.pos[k]);
    for (size_t v = 0; v < r.varying.size(); ++v)
        for (int k = 0; k < 4; ++k)
            r.varying[v][k] = a.varying[v][k]
                            + t * (b.varying[v][k] - a.varying[v][k]);
    return r;
}

template <typename DistFn>
void clip_against(const PolyBuf& in, PolyBuf& out, DistFn dist) {
    out.clear();
    if (in.empty()) return;
    for (size_t i = 0; i < in.size(); ++i) {
        const Vertex& a = in[i];
        const Vertex& b = in[(i + 1) % in.size()];
        const float da = dist(a.pos);
        const float db = dist(b.pos);
        if (da >= 0.0f) out.push_back(a);
        if ((da >= 0.0f) != (db >= 0.0f)) {
            // Edge crosses the plane.
            const float t = da / (da - db);
            out.push_back(lerp_vertex(a, b, t));
        }
    }
}

PolyBuf clip_triangle_z(const Vertex& a, const Vertex& b, const Vertex& c) {
    PolyBuf p = {a, b, c};
    PolyBuf q;
    // Near plane: z >= -w.
    clip_against(p, q, [](const Vec4f& v) { return v[2] + v[3]; });
    if (q.empty()) return q;
    // Far plane: z <= +w.
    clip_against(q, p, [](const Vec4f& v) { return v[3] - v[2]; });
    return p;
}


Vertex perspective_divide_and_viewport(const Context& ctx, const Vertex& clip) {
    Vertex out = clip;
    const float w = clip.pos[3];
    if (w == 0.0f) return out;     // degenerate; PA later cull
    const float inv_w = 1.0f / w;
    const float ndc_x = clip.pos[0] * inv_w;
    const float ndc_y = clip.pos[1] * inv_w;
    const float ndc_z = clip.pos[2] * inv_w;
    out.pos[0] = (ndc_x * 0.5f + 0.5f) * ctx.draw.vp_w + ctx.draw.vp_x;
    out.pos[1] = (ndc_y * 0.5f + 0.5f) * ctx.draw.vp_h + ctx.draw.vp_y;
    // Sprint 44 — clamp depth to [0,1]. The Sutherland-Hodgman clip
    // before this should already exclude z outside this range, but
    // numerical drift on edges can produce -ε / 1+ε; clamp keeps the
    // depth test stable.
    float z = ndc_z * 0.5f + 0.5f;
    if (z < 0.0f) z = 0.0f; else if (z > 1.0f) z = 1.0f;
    out.pos[2] = z;
    out.pos[3] = inv_w;                     // store 1/w for perspective interp
    return out;
}

bool back_face_cull(const Vertex& a, const Vertex& b, const Vertex& c, bool enable) {
    if (!enable) return false;
    const float ax = a.pos[0], ay = a.pos[1];
    const float bx = b.pos[0], by = b.pos[1];
    const float cx = c.pos[0], cy = c.pos[1];
    const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    return area <= 0.0f;            // CCW front-face convention
}

}  // namespace

void primitive_assembly(Context& ctx,
                        std::span<const Vertex> in_verts,
                        std::vector<Triangle>& out_tris) {
    out_tris.clear();
    if (in_verts.size() < 3) return;

    auto emit = [&](Vertex a, Vertex b, Vertex c) {
        // Clip the triangle against the near and far planes in clip
        // space first; the result is a convex polygon (3..5 verts).
        // Triangulate via fan from the first vertex.
        const PolyBuf clipped = clip_triangle_z(a, b, c);
        if (clipped.size() < 3) return;
        for (size_t i = 1; i + 1 < clipped.size(); ++i) {
            Vertex va = perspective_divide_and_viewport(ctx, clipped[0]);
            Vertex vb = perspective_divide_and_viewport(ctx, clipped[i]);
            Vertex vc = perspective_divide_and_viewport(ctx, clipped[i + 1]);
            if (back_face_cull(va, vb, vc, ctx.draw.cull_back)) continue;
            out_tris.push_back({{va, vb, vc}});
        }
    };

    switch (ctx.draw.primitive) {
        case DrawState::TRIANGLES:
            for (size_t i = 0; i + 2 < in_verts.size(); i += 3) {
                emit(in_verts[i], in_verts[i + 1], in_verts[i + 2]);
            }
            break;
        case DrawState::TRIANGLE_STRIP:
            for (size_t i = 0; i + 2 < in_verts.size(); ++i) {
                if (i & 1) emit(in_verts[i + 1], in_verts[i], in_verts[i + 2]);
                else       emit(in_verts[i], in_verts[i + 1], in_verts[i + 2]);
            }
            break;
        case DrawState::TRIANGLE_FAN:
            for (size_t i = 1; i + 1 < in_verts.size(); ++i) {
                emit(in_verts[0], in_verts[i], in_verts[i + 1]);
            }
            break;
    }
}

}  // namespace gpu::pipeline
