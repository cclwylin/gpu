#include "gpu/pipeline.h"

namespace gpu::pipeline {

void draw(Context& ctx, uint32_t vertex_count) {
    std::vector<std::array<Vec4f, 8>> attrs;
    vertex_fetch(ctx, attrs, vertex_count);

    std::vector<Vertex> verts;
    vertex_shader(ctx, attrs, verts);

    std::vector<Triangle> tris;
    primitive_assembly(ctx, verts, tris);

    std::vector<Quad> quads;
    rasterizer(ctx, tris, quads);

    for (auto& q : quads) {
        fragment_shader(ctx, q);
        per_fragment_ops(ctx, q);
    }

    resolve(ctx);
}

}  // namespace gpu::pipeline
