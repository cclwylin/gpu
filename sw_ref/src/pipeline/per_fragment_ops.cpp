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

// PFO writes fragment colour to:
//   - per-sample tile buffer (color_samples), if msaa_4x — resolve runs later
//   - the resolved framebuffer (color), otherwise
void per_fragment_ops(Context& ctx, const Quad& quad) {
    auto& fb = ctx.fb;
    for (const auto& f : quad.frags) {
        if (f.coverage_mask == 0) continue;
        if (f.pos.x < 0 || f.pos.x >= fb.width)  continue;
        if (f.pos.y < 0 || f.pos.y >= fb.height) continue;

        const uint32_t packed = pack_rgba8(f.varying[0]);
        const size_t pix = static_cast<size_t>(f.pos.y) * fb.width + f.pos.x;

        if (fb.msaa_4x) {
            const size_t base = pix * 4;
            for (int s = 0; s < 4; ++s) {
                if (f.coverage_mask & (1 << s)) {
                    fb.color_samples[base + s] = packed;
                }
            }
        } else {
            fb.color[pix] = packed;
        }
    }
}

}  // namespace gpu::pipeline
