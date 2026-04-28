#include "gpu/pipeline.h"

#include <cassert>
#include <cstring>
#include <vector>

#include "gpu_compiler/sim.h"

namespace gpu::pipeline {

namespace {
sim::Vec4 to_sim(const Vec4f& v) { return sim::Vec4{{v[0], v[1], v[2], v[3]}}; }
Vec4f      from_sim(const sim::Vec4& v) { return Vec4f{{v[0], v[1], v[2], v[3]}}; }
}

void vertex_shader(Context& ctx,
                   std::span<const std::array<Vec4f, 8>> in_attrs,
                   std::vector<Vertex>& out_verts) {
    out_verts.clear();
    out_verts.reserve(in_attrs.size());

    if (ctx.shaders.vs_binary != nullptr) {
        const auto* code = static_cast<const std::vector<uint64_t>*>(ctx.shaders.vs_binary);
        for (const auto& attrs : in_attrs) {
            sim::ThreadState t{};
            for (int i = 0; i < 32; ++i) t.c[i] = to_sim(ctx.draw.uniforms[i]);
            const int n = ctx.shaders.vs_attr_count > 0
                            ? ctx.shaders.vs_attr_count : 8;
            for (int i = 0; i < n; ++i) t.r[i] = to_sim(attrs[i]);

            auto er = sim::execute(*code, t);
            (void)er;

            Vertex out{};
            out.pos = from_sim(t.o[0]);
            const int vc = ctx.shaders.vs_varying_count;
            for (int i = 0; i < vc && i < 7; ++i) {
                out.varying[i] = from_sim(t.o[1 + i]);
            }
            out.varying_count = static_cast<uint8_t>(vc);
            out_verts.push_back(out);
        }
        return;
    }

    assert(ctx.shaders.vs && "vertex shader fn or binary must be bound");
    for (const auto& attrs : in_attrs) {
        Vertex v{};
        ctx.shaders.vs(ctx.draw, attrs.data(), v);
        out_verts.push_back(v);
    }
}

}  // namespace gpu::pipeline
