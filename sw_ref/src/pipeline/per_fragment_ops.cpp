#include "gpu/fp.h"
#include "gpu/pipeline.h"

#include <algorithm>

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

Vec4f unpack_rgba8(uint32_t px) {
    return {{
        static_cast<float>((px >>  0) & 0xFF) / 255.0f,
        static_cast<float>((px >>  8) & 0xFF) / 255.0f,
        static_cast<float>((px >> 16) & 0xFF) / 255.0f,
        static_cast<float>((px >> 24) & 0xFF) / 255.0f,
    }};
}

bool depth_pass(DrawState::DepthFunc func, float src, float dst) {
    using DF = DrawState;
    switch (func) {
        case DF::DF_LESS:     return src <  dst;
        case DF::DF_LEQUAL:   return src <= dst;
        case DF::DF_EQUAL:    return src == dst;
        case DF::DF_GEQUAL:   return src >= dst;
        case DF::DF_GREATER:  return src >  dst;
        case DF::DF_NOTEQUAL: return src != dst;
        case DF::DF_ALWAYS:   return true;
        case DF::DF_NEVER:    return false;
    }
    return true;
}

float blend_factor(DrawState::BlendFactor f, const Vec4f& src, const Vec4f& dst, int ch) {
    using BF = DrawState;
    switch (f) {
        case BF::BF_ZERO:                  return 0.0f;
        case BF::BF_ONE:                   return 1.0f;
        case BF::BF_SRC_COLOR:             return src[ch];
        case BF::BF_ONE_MINUS_SRC_COLOR:   return 1.0f - src[ch];
        case BF::BF_DST_COLOR:             return dst[ch];
        case BF::BF_ONE_MINUS_DST_COLOR:   return 1.0f - dst[ch];
        case BF::BF_SRC_ALPHA:             return src[3];
        case BF::BF_ONE_MINUS_SRC_ALPHA:   return 1.0f - src[3];
        case BF::BF_DST_ALPHA:             return dst[3];
        case BF::BF_ONE_MINUS_DST_ALPHA:   return 1.0f - dst[3];
    }
    return 0.0f;
}

Vec4f apply_blend(const DrawState& s, const Vec4f& src, const Vec4f& dst) {
    Vec4f out;
    for (int ch = 0; ch < 4; ++ch) {
        float fs = blend_factor(s.blend_src, src, dst, ch);
        float fd = blend_factor(s.blend_dst, src, dst, ch);
        float a = src[ch] * fs;
        float b = dst[ch] * fd;
        switch (s.blend_eq) {
            case DrawState::BE_ADD:              out[ch] = a + b;          break;
            case DrawState::BE_SUBTRACT:         out[ch] = a - b;          break;
            case DrawState::BE_REVERSE_SUBTRACT: out[ch] = b - a;          break;
        }
    }
    return out;
}

}  // namespace

// PFO with depth test + alpha blend.
// MSAA path uses depth_samples / color_samples (per sample); 1× uses depth + color.
// Stencil deferred to a follow-up sprint.
void per_fragment_ops(Context& ctx, const Quad& quad) {
    auto& fb  = ctx.fb;
    auto& ds  = ctx.draw;

    for (const auto& f : quad.frags) {
        if (f.coverage_mask == 0) continue;
        if (f.pos.x < 0 || f.pos.x >= fb.width)  continue;
        if (f.pos.y < 0 || f.pos.y >= fb.height) continue;

        const size_t pix = static_cast<size_t>(f.pos.y) * fb.width + f.pos.x;

        if (fb.msaa_4x) {
            const size_t base = pix * 4;
            uint8_t mask = f.coverage_mask;
            for (int s = 0; s < 4; ++s) {
                if (!(mask & (1 << s))) continue;
                if (ds.depth_test && !fb.depth_samples.empty()) {
                    if (!depth_pass(ds.depth_func, f.depth, fb.depth_samples[base + s])) {
                        continue;
                    }
                    if (ds.depth_write) fb.depth_samples[base + s] = f.depth;
                }
                Vec4f src = f.varying[0];
                if (ds.blend_enable) {
                    Vec4f dst = unpack_rgba8(fb.color_samples[base + s]);
                    src = apply_blend(ds, src, dst);
                }
                fb.color_samples[base + s] = pack_rgba8(src);
            }
        } else {
            if (ds.depth_test && !fb.depth.empty()) {
                if (!depth_pass(ds.depth_func, f.depth, fb.depth[pix])) continue;
                if (ds.depth_write) fb.depth[pix] = f.depth;
            }
            Vec4f src = f.varying[0];
            if (ds.blend_enable) {
                Vec4f dst = unpack_rgba8(fb.color[pix]);
                src = apply_blend(ds, src, dst);
            }
            fb.color[pix] = pack_rgba8(src);
        }
    }
}

}  // namespace gpu::pipeline
