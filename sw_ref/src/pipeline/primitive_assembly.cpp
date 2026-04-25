#include "gpu/pipeline.h"

namespace gpu::pipeline {

namespace {

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
    out.pos[2] = ndc_z * 0.5f + 0.5f;       // depth in [0,1]
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
        a = perspective_divide_and_viewport(ctx, a);
        b = perspective_divide_and_viewport(ctx, b);
        c = perspective_divide_and_viewport(ctx, c);
        if (back_face_cull(a, b, c, ctx.draw.cull_back)) return;
        out_tris.push_back({{a, b, c}});
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
