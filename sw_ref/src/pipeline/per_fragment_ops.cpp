#include "gpu/fp.h"
#include "gpu/pipeline.h"

namespace gpu::pipeline {

namespace {
uint32_t pack_rgba8(const Vec4f& c) {
    auto to_u8 = [](float f) -> uint32_t {
        const float s = fp::sat(f);
        return static_cast<uint32_t>(s * 255.0f + 0.5f) & 0xFF;
    };
    return (to_u8(c[3]) << 24) | (to_u8(c[2]) << 16) |
           (to_u8(c[1]) <<  8) |  to_u8(c[0]);
}
}  // namespace

// Skeleton PFO: depth/stencil disabled by default; no blending; no MSAA path.
// Writes directly to the resolved framebuffer.
void per_fragment_ops(Context& ctx, const Quad& quad) {
    auto& fb = ctx.fb;
    for (const auto& f : quad.frags) {
        if (f.coverage_mask == 0) continue;
        if (f.pos.x < 0 || f.pos.x >= fb.width)  continue;
        if (f.pos.y < 0 || f.pos.y >= fb.height) continue;
        const size_t idx = static_cast<size_t>(f.pos.y) * fb.width + f.pos.x;
        fb.color[idx] = pack_rgba8(f.varying[0]);
    }
}

}  // namespace gpu::pipeline
