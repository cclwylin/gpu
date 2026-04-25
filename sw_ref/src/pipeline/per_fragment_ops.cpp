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

bool stencil_pass(DrawState::StencilFunc func, uint8_t ref_masked, uint8_t cur_masked) {
    using SF = DrawState;
    switch (func) {
        case SF::SF_NEVER:    return false;
        case SF::SF_LESS:     return ref_masked <  cur_masked;
        case SF::SF_LEQUAL:   return ref_masked <= cur_masked;
        case SF::SF_GREATER:  return ref_masked >  cur_masked;
        case SF::SF_GEQUAL:   return ref_masked >= cur_masked;
        case SF::SF_EQUAL:    return ref_masked == cur_masked;
        case SF::SF_NOTEQUAL: return ref_masked != cur_masked;
        case SF::SF_ALWAYS:   return true;
    }
    return true;
}

uint8_t apply_stencil_op(DrawState::StencilOp op, uint8_t cur, uint8_t ref) {
    using SO = DrawState;
    switch (op) {
        case SO::SO_KEEP:    return cur;
        case SO::SO_ZERO:    return 0;
        case SO::SO_REPLACE: return ref;
        case SO::SO_INCR:    return cur < 0xFF ? static_cast<uint8_t>(cur + 1) : 0xFF;
        case SO::SO_DECR:    return cur > 0    ? static_cast<uint8_t>(cur - 1) : 0;
        case SO::SO_INVERT:  return static_cast<uint8_t>(~cur);
    }
    return cur;
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

// Alpha-to-coverage (Sprint 17). Maps FS alpha [0,1] to a 4-bit MSAA mask
// using a fixed monotonic table (per docs/msaa_spec.md §5.2).
uint8_t a2c_mask(float alpha) {
    if (alpha <  0.125f) return 0b0000;
    if (alpha <  0.375f) return 0b0001;
    if (alpha <  0.625f) return 0b0101;
    if (alpha <  0.875f) return 0b0111;
    return 0b1111;
}

}  // namespace

// PFO with stencil + depth + a2c + blend, both 1× and 4× MSAA paths.
void per_fragment_ops(Context& ctx, const Quad& quad) {
    auto& fb = ctx.fb;
    auto& ds = ctx.draw;
    const uint8_t s_ref_masked = ds.stencil_ref & ds.stencil_read_mask;

    for (const auto& f : quad.frags) {
        if (f.coverage_mask == 0) continue;
        if (f.pos.x < 0 || f.pos.x >= fb.width)  continue;
        if (f.pos.y < 0 || f.pos.y >= fb.height) continue;

        uint8_t mask  = f.coverage_mask;
        Vec4f   color = f.varying[0];

        if (fb.msaa_4x && ds.a2c) mask &= a2c_mask(color[3]);

        const size_t pix = static_cast<size_t>(f.pos.y) * fb.width + f.pos.x;

        if (fb.msaa_4x) {
            const size_t base = pix * 4;
            for (int s = 0; s < 4; ++s) {
                if (!(mask & (1 << s))) continue;
                bool s_pass = true;
                uint8_t s_cur = 0;
                if (ds.stencil_test && !fb.stencil_samples.empty()) {
                    s_cur = fb.stencil_samples[base + s];
                    s_pass = stencil_pass(ds.stencil_func, s_ref_masked,
                                          static_cast<uint8_t>(s_cur & ds.stencil_read_mask));
                }
                bool z_pass = true;
                if (ds.depth_test && !fb.depth_samples.empty()) {
                    z_pass = depth_pass(ds.depth_func, f.depth, fb.depth_samples[base + s]);
                }
                if (ds.stencil_test) {
                    DrawState::StencilOp op = !s_pass ? ds.sop_fail
                                            : !z_pass ? ds.sop_zfail
                                                      : ds.sop_zpass;
                    uint8_t s_new = apply_stencil_op(op, s_cur, ds.stencil_ref);
                    s_new = (s_cur & ~ds.stencil_write_mask) | (s_new & ds.stencil_write_mask);
                    if (!fb.stencil_samples.empty()) fb.stencil_samples[base + s] = s_new;
                }
                if (!s_pass || !z_pass) continue;
                if (ds.depth_write && !fb.depth_samples.empty())
                    fb.depth_samples[base + s] = f.depth;
                Vec4f src = color;
                if (ds.blend_enable) {
                    Vec4f dst = unpack_rgba8(fb.color_samples[base + s]);
                    src = apply_blend(ds, src, dst);
                }
                fb.color_samples[base + s] = pack_rgba8(src);
            }
        } else {
            bool s_pass = true;
            uint8_t s_cur = 0;
            if (ds.stencil_test && !fb.stencil.empty()) {
                s_cur = fb.stencil[pix];
                s_pass = stencil_pass(ds.stencil_func, s_ref_masked,
                                      static_cast<uint8_t>(s_cur & ds.stencil_read_mask));
            }
            bool z_pass = true;
            if (ds.depth_test && !fb.depth.empty()) {
                z_pass = depth_pass(ds.depth_func, f.depth, fb.depth[pix]);
            }
            if (ds.stencil_test) {
                DrawState::StencilOp op = !s_pass ? ds.sop_fail
                                        : !z_pass ? ds.sop_zfail
                                                  : ds.sop_zpass;
                uint8_t s_new = apply_stencil_op(op, s_cur, ds.stencil_ref);
                s_new = (s_cur & ~ds.stencil_write_mask) | (s_new & ds.stencil_write_mask);
                if (!fb.stencil.empty()) fb.stencil[pix] = s_new;
            }
            if (!s_pass || !z_pass) continue;
            if (ds.depth_write && !fb.depth.empty()) fb.depth[pix] = f.depth;

            Vec4f src = color;
            if (ds.blend_enable) {
                Vec4f dst = unpack_rgba8(fb.color[pix]);
                src = apply_blend(ds, src, dst);
            }
            fb.color[pix] = pack_rgba8(src);
        }
    }
}

}  // namespace gpu::pipeline
