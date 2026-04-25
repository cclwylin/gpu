#include "gpu/pipeline.h"

#include <cassert>

namespace gpu::pipeline {

void fragment_shader(Context& ctx, Quad& quad) {
    assert(ctx.shaders.fs && "fragment shader fn must be bound");
    for (auto& f : quad.frags) {
        if (f.coverage_mask == 0) continue;
        Vec4f color{};
        ctx.shaders.fs(ctx.draw, f.varying.data(), color);
        // Pack color back into varying[0] for PFO to consume.
        // (skeleton: we re-use varying[0] as the FS output channel.)
        f.varying[0] = color;
    }
}

}  // namespace gpu::pipeline
