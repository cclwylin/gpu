#include "gpu/pipeline.h"

namespace gpu::pipeline {

// Box-filter resolve: 4 samples per pixel -> 1 RGBA8 averaged with
// round-to-nearest. Matches docs/msaa_spec.md §9.
//
// Per-channel: out = (s0 + s1 + s2 + s3 + 2) >> 2
void resolve(Context& ctx) {
    auto& fb = ctx.fb;
    if (!fb.msaa_4x) return;

    auto channel = [](uint32_t px, int ch) -> uint32_t {
        return (px >> (ch * 8)) & 0xFFu;
    };

    for (int y = 0; y < fb.height; ++y) {
        for (int x = 0; x < fb.width; ++x) {
            const size_t pix = static_cast<size_t>(y) * fb.width + x;
            const size_t base = pix * 4;
            uint32_t out = 0;
            for (int ch = 0; ch < 4; ++ch) {
                uint32_t sum = channel(fb.color_samples[base + 0], ch) +
                               channel(fb.color_samples[base + 1], ch) +
                               channel(fb.color_samples[base + 2], ch) +
                               channel(fb.color_samples[base + 3], ch);
                uint32_t v = (sum + 2) >> 2;     // round-to-nearest
                out |= (v & 0xFFu) << (ch * 8);
            }
            fb.color[pix] = out;
        }
    }
}

}  // namespace gpu::pipeline
