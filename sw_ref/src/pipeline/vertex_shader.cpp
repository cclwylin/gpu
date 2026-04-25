#include "gpu/pipeline.h"

#include <cassert>

namespace gpu::pipeline {

void vertex_shader(Context& ctx,
                   std::span<const std::array<Vec4f, 8>> in_attrs,
                   std::vector<Vertex>& out_verts) {
    assert(ctx.shaders.vs && "vertex shader fn must be bound");
    out_verts.clear();
    out_verts.reserve(in_attrs.size());
    for (const auto& attrs : in_attrs) {
        Vertex v{};
        ctx.shaders.vs(ctx.draw, attrs.data(), v);
        out_verts.push_back(v);
    }
}

}  // namespace gpu::pipeline
