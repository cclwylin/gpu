#include "gpu/pipeline.h"

#include <cassert>
#include <vector>

#include "gpu_compiler/sim.h"

namespace gpu::pipeline {

namespace {
sim::Vec4 to_sim(const Vec4f& v) { return sim::Vec4{{v[0], v[1], v[2], v[3]}}; }
Vec4f      from_sim(const sim::Vec4& v) { return Vec4f{{v[0], v[1], v[2], v[3]}}; }
}

void fragment_shader(Context& ctx, Quad& quad) {
    if (ctx.shaders.fs_binary != nullptr) {
        const auto* code = static_cast<const std::vector<uint64_t>*>(ctx.shaders.fs_binary);
        for (auto& f : quad.frags) {
            if (f.coverage_mask == 0) continue;

            sim::ThreadState t{};
            for (int i = 0; i < 16; ++i) t.c[i] = to_sim(ctx.draw.uniforms[i]);
            const int vc = ctx.shaders.fs_varying_count;
            for (int i = 0; i < vc && i < 8; ++i) t.varying[i] = to_sim(f.varying[i]);

            sim::execute(*code, t);

            // FS lane is killed by `kil` (clears coverage).
            if (!t.lane_active) {
                f.coverage_mask = 0;
                continue;
            }
            f.varying[0] = from_sim(t.o[0]);
        }
        return;
    }

    assert(ctx.shaders.fs && "fragment shader fn or binary must be bound");
    for (auto& f : quad.frags) {
        if (f.coverage_mask == 0) continue;
        Vec4f color{};
        ctx.shaders.fs(ctx.draw, f.varying.data(), color);
        f.varying[0] = color;
    }
}

}  // namespace gpu::pipeline
