// Sprint 36/37/38 — glEnd → render via gpu::pipeline (sw_ref).
//
// Takes the buffered immediate-mode vertices, applies MVP, optionally
// runs Gouraud lighting, triangulates per the begin-mode, and submits
// to gpu::pipeline::draw with auto-generated VS/FS bytecode.

#include "glcompat_runtime.h"

#include <GL/gl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

namespace glcompat {

namespace {

// Gouraud lighting per vertex. Returns the vertex's lit RGBA.
Vec4 gouraud(const ImmVertex& iv, const Mat4& mv, const Mat4& mv_inv_t) {
    auto& s = state();

    // Eye-space position and normal.
    Vec4 epos4 = mat4_apply(mv, iv.pos);
    const float ex = epos4[0], ey = epos4[1], ez = epos4[2];
    const Vec4 n4 = mat4_apply(mv_inv_t,
                               {{iv.normal[0], iv.normal[1], iv.normal[2], 0.0f}});
    float nx = n4[0], ny = n4[1], nz = n4[2];
    const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nl > 0.0f) { nx /= nl; ny /= nl; nz /= nl; }

    // Material: when COLOR_MATERIAL is on, current color overrides
    // ambient+diffuse for this vertex.
    Material mat = s.material;
    if (s.color_material) {
        mat.ambient = iv.color;
        mat.diffuse = iv.color;
    }

    Vec4 c = mat.emission;
    // Light-model ambient.
    for (int k = 0; k < 4; ++k)
        c[k] += s.light_model_ambient[k] * mat.ambient[k];

    for (const auto& l : s.lights) {
        if (!l.enabled) continue;
        // Direction to light (eye space). Directional if .w == 0.
        float lx, ly, lz;
        if (l.position[3] == 0.0f) {
            lx = l.position[0]; ly = l.position[1]; lz = l.position[2];
        } else {
            lx = l.position[0] - ex;
            ly = l.position[1] - ey;
            lz = l.position[2] - ez;
        }
        const float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
        if (ll > 0.0f) { lx /= ll; ly /= ll; lz /= ll; }

        // Ambient contribution.
        for (int k = 0; k < 4; ++k) c[k] += l.ambient[k] * mat.ambient[k];

        // Diffuse.
        const float ndotl = std::max(0.0f, nx * lx + ny * ly + nz * lz);
        if (ndotl > 0.0f) {
            for (int k = 0; k < 4; ++k)
                c[k] += l.diffuse[k] * mat.diffuse[k] * ndotl;

            // Specular (Blinn-Phong half-vector, infinite viewer).
            if (mat.shininess > 0.0f) {
                float hx = lx, hy = ly, hz = lz + 1.0f;
                const float hl = std::sqrt(hx * hx + hy * hy + hz * hz);
                if (hl > 0.0f) { hx /= hl; hy /= hl; hz /= hl; }
                const float ndoth = std::max(0.0f, nx * hx + ny * hy + nz * hz);
                const float spec = std::pow(ndoth, mat.shininess);
                for (int k = 0; k < 4; ++k)
                    c[k] += l.specular[k] * mat.specular[k] * spec;
            }
        }
    }

    // Saturate to [0,1]. Alpha taken from material diffuse.
    c[3] = mat.diffuse[3];
    for (int k = 0; k < 4; ++k) c[k] = c[k] < 0.0f ? 0.0f : (c[k] > 1.0f ? 1.0f : c[k]);
    return c;
}

// Cheap inverse-transpose of upper-left 3x3 of a Mat4 (good enough for
// rotation+uniform scale; non-uniform scale will distort normals — out
// of scope for the example corpus).
Mat4 mat3_inv_transpose(const Mat4& m) {
    Mat4 r = mat4_identity();
    // Compute determinant of upper-left 3x3.
    float a = m[0], b = m[4], c = m[8];
    float d = m[1], e = m[5], f = m[9];
    float g = m[2], h = m[6], i = m[10];
    float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (det == 0.0f) return mat4_identity();
    const float inv = 1.0f / det;
    // Transpose of cofactor / det.
    r[0]  = (e * i - f * h) * inv;
    r[1]  = (c * h - b * i) * inv;
    r[2]  = (b * f - c * e) * inv;
    r[4]  = (f * g - d * i) * inv;
    r[5]  = (a * i - c * g) * inv;
    r[6]  = (c * d - a * f) * inv;
    r[8]  = (d * h - e * g) * inv;
    r[9]  = (b * g - a * h) * inv;
    r[10] = (a * e - b * d) * inv;
    // Transpose for normal transform.
    std::swap(r[1], r[4]);
    std::swap(r[2], r[8]);
    std::swap(r[6], r[9]);
    return r;
}

// Triangulate the current primitive into a flat triangle list.
// Each output entry is 3 indices into `verts`.
std::vector<std::array<int, 3>> triangulate(GLenum prim, int n) {
    std::vector<std::array<int, 3>> tris;
    if (n < 3) return tris;
    switch (prim) {
        case GL_TRIANGLES:
            for (int i = 0; i + 2 < n; i += 3) tris.push_back({i, i+1, i+2});
            break;
        case GL_TRIANGLE_STRIP:
            for (int i = 0; i + 2 < n; ++i) {
                if (i & 1) tris.push_back({i+1, i, i+2});
                else       tris.push_back({i, i+1, i+2});
            }
            break;
        case GL_TRIANGLE_FAN:
        case GL_POLYGON:
            for (int i = 1; i + 1 < n; ++i) tris.push_back({0, i, i+1});
            break;
        case GL_QUADS:
            for (int i = 0; i + 3 < n; i += 4) {
                tris.push_back({i, i+1, i+2});
                tris.push_back({i, i+2, i+3});
            }
            break;
        case GL_QUAD_STRIP:
            for (int i = 0; i + 3 < n; i += 2) {
                tris.push_back({i, i+1, i+3});
                tris.push_back({i, i+3, i+2});
            }
            break;
        // Lines, line strips, line loops — rendered as 1-pixel-wide
        // axis-aligned quads. Not real anti-aliased GL_LINE_SMOOTH;
        // good enough for stroke fonts and outline tests.
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
            // Caller handles line→quad expansion before triangulate;
            // see flush_immediate.
            break;
        default:
            break;
    }
    return tris;
}

// Expand a line primitive into a triangle list of thin quads. Each
// segment becomes a 1.5-pixel-wide screen-space ribbon. Operates on
// `lit` (clip space) → returns added clip-space verts + per-vert color
// + new triangle indices.
struct LineExpansion {
    std::vector<Vec4>            extra_clip;
    std::vector<Vec4>            extra_color;
    std::vector<std::array<int, 3>> tris;
};

LineExpansion expand_lines(GLenum prim, const std::vector<Vec4>& clip,
                           const std::vector<Vec4>& color,
                           int W, int H) {
    LineExpansion x;
    auto add_seg = [&](int ia, int ib) {
        // Project to screen-space to compute a perpendicular offset.
        const Vec4& a = clip[ia]; const Vec4& b = clip[ib];
        const float aw = a[3] != 0 ? a[3] : 1, bw = b[3] != 0 ? b[3] : 1;
        const float ax = (a[0]/aw * 0.5f + 0.5f) * W;
        const float ay = (a[1]/aw * 0.5f + 0.5f) * H;
        const float bx = (b[0]/bw * 0.5f + 0.5f) * W;
        const float by = (b[1]/bw * 0.5f + 0.5f) * H;
        float dx = bx - ax, dy = by - ay;
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-3f) return;
        // Perpendicular, in pixels → back to NDC offset.
        const float halfw_px = 0.75f;
        const float px = -dy / len * halfw_px, py = dx / len * halfw_px;
        const float ndx = px / W * 2.0f, ndy = py / H * 2.0f;
        // Generate 4 verts (a-d, a+d, b+d, b-d) preserving w.
        auto v = [&](const Vec4& p, float ox, float oy) {
            const float w = p[3] != 0 ? p[3] : 1;
            Vec4 r = {{p[0] + ox * w, p[1] + oy * w, p[2], p[3]}};
            return r;
        };
        const int base = (int)x.extra_clip.size();
        x.extra_clip.push_back(v(a, -ndx, -ndy)); x.extra_color.push_back(color[ia]);
        x.extra_clip.push_back(v(a,  ndx,  ndy)); x.extra_color.push_back(color[ia]);
        x.extra_clip.push_back(v(b,  ndx,  ndy)); x.extra_color.push_back(color[ib]);
        x.extra_clip.push_back(v(b, -ndx, -ndy)); x.extra_color.push_back(color[ib]);
        x.tris.push_back({base, base+1, base+2});
        x.tris.push_back({base, base+2, base+3});
    };
    const int n = (int)clip.size();
    if (prim == GL_LINES) {
        for (int i = 0; i + 1 < n; i += 2) add_seg(i, i + 1);
    } else if (prim == GL_LINE_STRIP) {
        for (int i = 0; i + 1 < n; ++i) add_seg(i, i + 1);
    } else if (prim == GL_LINE_LOOP && n >= 2) {
        for (int i = 0; i + 1 < n; ++i) add_seg(i, i + 1);
        add_seg(n - 1, 0);
    }
    return x;
}

// Cache a passthrough VS+FS pair compiled from inline assembly.
struct ShaderPair {
    std::vector<uint64_t> vs;
    std::vector<uint64_t> fs;
};
const ShaderPair& shader_pair() {
    static ShaderPair sp = []{
        ShaderPair p;
        auto va = gpu::asm_::assemble("mov o0, r0\nmov o1, r1\n");
        auto fa = gpu::asm_::assemble("mov o0, v0\n");
        p.vs.assign(va.code.begin(), va.code.end());
        p.fs.assign(fa.code.begin(), fa.code.end());
        return p;
    }();
    return sp;
}

}  // namespace

// Captured scene as an ordered list of ops so mid-frame state changes
// AND mid-frame fb mutations (glClear with a different clear-color,
// scissor toggles, etc.) replay in the same order they happened. Two
// op kinds today:
//   * BATCH — pipeline draw with state snapshot + verts. Consecutive
//     flushes with identical state and no intervening clear coalesce.
//   * CLEAR — fill the whole fb with a packed RGBA8 colour (mirrors
//     glcompat's current "scissor-ignored" glClear semantics; once
//     glScissor lands here it'll grow x/y/w/h fields).
namespace {
// Sprint 61 — `vars[]` carries up to 7 vec4 varyings per vertex
// (matching the runtime Vertex.varying[7] capacity). Pre-Sprint-61 the
// scene format only had a single `col` slot, so multi-varying shaders
// (the 1060-case `fragment_ops.blend.*` block) couldn't roundtrip
// through the SC chain. `n_vars` records how many of the 7 slots the
// VS actually wrote so the writer / SC replay only ship the active
// varyings.
struct SceneVert {
    Vec4               pos;
    std::array<Vec4,7> vars{};
};
struct SceneBatch {
    bool        depth_test  = false;
    bool        depth_write = true;
    const char* depth_func  = "less";
    bool        cull_back   = false;
    bool        blend       = false;
    bool        stencil_test = false;
    const char* stencil_func = "always";
    int         stencil_ref  = 0;
    int         stencil_read_mask  = 0xFF;
    int         stencil_write_mask = 0xFF;
    const char* stencil_sfail  = "keep";
    const char* stencil_dpfail = "keep";
    const char* stencil_dppass = "keep";
    // Sprint 61 — full blend state: separate RGB / alpha factors,
    // equations, and the blend color. Sprint 60 only emitted the
    // boolean enable, so the SC replay always blended with the default
    // SRC_ALPHA / ONE_MINUS_SRC_ALPHA → 1060 fragment_ops.blend.* cases
    // diverged from sw_ref. Strings match `apply_batch_state`'s map.
    const char* blend_src_rgb   = "src_alpha";
    const char* blend_dst_rgb   = "one_minus_src_alpha";
    const char* blend_src_alpha = "src_alpha";
    const char* blend_dst_alpha = "one_minus_src_alpha";
    const char* blend_eq_rgb    = "add";
    const char* blend_eq_alpha  = "add";
    float       blend_color[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
    int                     vp_x = 0, vp_y = 0, vp_w = 0, vp_h = 0;  // Sprint 61
    int                     n_vars = 1;       // Sprint 61 — active varying count (1..7)
    std::vector<SceneVert>  verts;
};
struct SceneBitmap {
    int      x = 0, y = 0, w = 0, h = 0;
    uint32_t color = 0;          // packed AABBGGRR
    std::vector<uint8_t> bits;   // raw glBitmap bytes (MSB-first, rows bottom-up, (w+7)/8 bytes per row)
};
struct SceneOp {
    enum Kind { BATCH, CLEAR, BITMAP, CLEAR_DEPTH, CLEAR_STENCIL } kind = BATCH;
    SceneBatch  batch;       // valid when kind == BATCH
    uint32_t    clear_rgba = 0;   // valid when kind == CLEAR (AABBGGRR)
    SceneBitmap bitmap;      // valid when kind == BITMAP
    float       clear_depth = 1.0f;     // valid when kind == CLEAR_DEPTH
    uint8_t     clear_stencil_val = 0;  // valid when kind == CLEAR_STENCIL
    // Sprint 60 — per-CLEAR scissor box + color-mask lane. The replay
    // applies `pix = (old & ~lane) | (rgba & lane)` per pixel inside
    // the rect. `clear_rect_full = true` means whole-fb clear (the
    // legacy behaviour); writers default to that and only flip when
    // GL_SCISSOR_TEST is on or glColorMask isn't all-true. Old scene
    // files (with the bare `clear_rect <rgba>` form) still parse — the
    // extra fields default to whole-fb / 0xFFFFFFFF lane.
    bool        clear_rect_full = true;
    int         clear_x0 = 0, clear_y0 = 0, clear_x1 = 0, clear_y1 = 0;
    uint32_t    clear_lane = 0xFFFFFFFFu;
};
std::vector<SceneOp> g_scene_ops;

const char* depth_func_name(GLenum f) {
    switch (f) {
        case GL_NEVER:    return "never";
        case GL_LESS:     return "less";
        case GL_LEQUAL:   return "lequal";
        case GL_EQUAL:    return "equal";
        case GL_GEQUAL:   return "gequal";
        case GL_GREATER:  return "greater";
        case GL_NOTEQUAL: return "notequal";
        case GL_ALWAYS:   return "always";
        default:          return "less";
    }
}
const char* stencil_func_name(GLenum f) {
    switch (f) {
        case GL_NEVER:    return "never";
        case GL_LESS:     return "less";
        case GL_LEQUAL:   return "lequal";
        case GL_GREATER:  return "greater";
        case GL_GEQUAL:   return "gequal";
        case GL_EQUAL:    return "equal";
        case GL_NOTEQUAL: return "notequal";
        case GL_ALWAYS:   return "always";
        default:          return "always";
    }
}
const char* stencil_op_name(GLenum o) {
    switch (o) {
        case GL_KEEP:    return "keep";
        case GL_ZERO:    return "zero";
        case GL_REPLACE: return "replace";
        case GL_INCR:    return "incr";
        case GL_DECR:    return "decr";
        case GL_INVERT:  return "invert";
        default:         return "keep";
    }
}
// Sprint 61 — BlendFactor / BlendEq → token, paired with the inverse
// table in sc_pattern_runner.cpp::apply_batch_state. Stable strings so
// captured scenes survive across format-rev bumps.
const char* blend_factor_name(gpu::DrawState::BlendFactor f) {
    using DF = gpu::DrawState;
    switch (f) {
        case DF::BF_ZERO:                     return "zero";
        case DF::BF_ONE:                      return "one";
        case DF::BF_SRC_COLOR:                return "src_color";
        case DF::BF_ONE_MINUS_SRC_COLOR:      return "one_minus_src_color";
        case DF::BF_DST_COLOR:                return "dst_color";
        case DF::BF_ONE_MINUS_DST_COLOR:      return "one_minus_dst_color";
        case DF::BF_SRC_ALPHA:                return "src_alpha";
        case DF::BF_ONE_MINUS_SRC_ALPHA:      return "one_minus_src_alpha";
        case DF::BF_DST_ALPHA:                return "dst_alpha";
        case DF::BF_ONE_MINUS_DST_ALPHA:      return "one_minus_dst_alpha";
        case DF::BF_CONSTANT_COLOR:           return "constant_color";
        case DF::BF_ONE_MINUS_CONSTANT_COLOR: return "one_minus_constant_color";
        case DF::BF_CONSTANT_ALPHA:           return "constant_alpha";
        case DF::BF_ONE_MINUS_CONSTANT_ALPHA: return "one_minus_constant_alpha";
        case DF::BF_SRC_ALPHA_SATURATE:       return "src_alpha_saturate";
    }
    return "src_alpha";
}
const char* blend_eq_name(gpu::DrawState::BlendEq e) {
    switch (e) {
        case gpu::DrawState::BE_ADD:              return "add";
        case gpu::DrawState::BE_SUBTRACT:         return "subtract";
        case gpu::DrawState::BE_REVERSE_SUBTRACT: return "reverse_subtract";
    }
    return "add";
}
SceneBatch& open_or_reuse_batch(const State& s) {
    SceneBatch cur;
    cur.depth_test  = s.depth_test;
    cur.depth_write = s.depth_write;
    cur.depth_func  = depth_func_name(s.depth_func);
    cur.cull_back   = s.cull_face && s.cull_mode == GL_BACK;
    cur.blend       = s.blend;
    cur.stencil_test = s.stencil_test;
    cur.stencil_func = stencil_func_name(s.stencil_func);
    cur.stencil_ref  = s.stencil_ref;
    cur.stencil_read_mask  = (int)(s.stencil_read_mask  & 0xFF);
    cur.stencil_write_mask = (int)(s.stencil_write_mask & 0xFF);
    cur.stencil_sfail  = stencil_op_name(s.stencil_sfail);
    cur.stencil_dpfail = stencil_op_name(s.stencil_dpfail);
    cur.stencil_dppass = stencil_op_name(s.stencil_dppass);
    // Sprint 61 — capture full blend state from the live DrawState so
    // the SC replay can reproduce arbitrary glBlendFuncSeparate /
    // glBlendEquationSeparate / glBlendColor combinations.
    cur.blend_src_rgb   = blend_factor_name(s.ctx.draw.blend_src_rgb);
    cur.blend_dst_rgb   = blend_factor_name(s.ctx.draw.blend_dst_rgb);
    cur.blend_src_alpha = blend_factor_name(s.ctx.draw.blend_src_alpha);
    cur.blend_dst_alpha = blend_factor_name(s.ctx.draw.blend_dst_alpha);
    cur.blend_eq_rgb    = blend_eq_name(s.ctx.draw.blend_eq_rgb);
    cur.blend_eq_alpha  = blend_eq_name(s.ctx.draw.blend_eq_alpha);
    for (int i = 0; i < 4; ++i)
        cur.blend_color[i] = s.ctx.draw.blend_color[i];
    cur.vp_x = s.vp_x; cur.vp_y = s.vp_y;
    cur.vp_w = s.vp_w; cur.vp_h = s.vp_h;
    if (!g_scene_ops.empty() && g_scene_ops.back().kind == SceneOp::BATCH) {
        const SceneBatch& last = g_scene_ops.back().batch;
        if (last.depth_test  == cur.depth_test  &&
            last.depth_write == cur.depth_write &&
            std::strcmp(last.depth_func, cur.depth_func) == 0 &&
            last.cull_back   == cur.cull_back   &&
            last.blend       == cur.blend       &&
            std::strcmp(last.blend_src_rgb,   cur.blend_src_rgb)   == 0 &&
            std::strcmp(last.blend_dst_rgb,   cur.blend_dst_rgb)   == 0 &&
            std::strcmp(last.blend_src_alpha, cur.blend_src_alpha) == 0 &&
            std::strcmp(last.blend_dst_alpha, cur.blend_dst_alpha) == 0 &&
            std::strcmp(last.blend_eq_rgb,    cur.blend_eq_rgb)    == 0 &&
            std::strcmp(last.blend_eq_alpha,  cur.blend_eq_alpha)  == 0 &&
            last.blend_color[0] == cur.blend_color[0] &&
            last.blend_color[1] == cur.blend_color[1] &&
            last.blend_color[2] == cur.blend_color[2] &&
            last.blend_color[3] == cur.blend_color[3] &&
            last.vp_x == cur.vp_x && last.vp_y == cur.vp_y &&
            last.vp_w == cur.vp_w && last.vp_h == cur.vp_h &&
            last.stencil_test == cur.stencil_test &&
            std::strcmp(last.stencil_func, cur.stencil_func) == 0 &&
            last.stencil_ref == cur.stencil_ref &&
            last.stencil_read_mask  == cur.stencil_read_mask  &&
            last.stencil_write_mask == cur.stencil_write_mask &&
            std::strcmp(last.stencil_sfail,  cur.stencil_sfail)  == 0 &&
            std::strcmp(last.stencil_dpfail, cur.stencil_dpfail) == 0 &&
            std::strcmp(last.stencil_dppass, cur.stencil_dppass) == 0) {
            return g_scene_ops.back().batch;
        }
    }
    SceneOp op;
    op.kind  = SceneOp::BATCH;
    op.batch = std::move(cur);
    g_scene_ops.push_back(std::move(op));
    return g_scene_ops.back().batch;
}
}  // namespace

// Called from glClear(GL_COLOR_BUFFER_BIT) when scene capture is on.
// Records a clear op so the SC chain can apply it in the same order
// the live glcompat fb saw it (between mid-frame draw batches).
//
// Sprint 60 overload — also accepts the scissor rect that limited the
// clear and the bitwise color-mask `lane`. Replayers apply
// `pix = (old & ~lane) | (rgba & lane)` per pixel inside [x0,x1)×[y0,y1).
// `full=true` means whole-fb clear (uses the legacy fast-path on the
// SC side). The rgba8 still has the test's clear color *unmodified* —
// the lane is what carries glColorMask. Without this, scissored /
// masked color_clear cases were diverging from sw_ref by ~100 RMSE in
// the SC sweep.
void scene_record_clear(uint32_t rgba8) {
    SceneOp op;
    op.kind = SceneOp::CLEAR;
    op.clear_rgba = rgba8;
    g_scene_ops.push_back(std::move(op));
}
void scene_record_clear_rect(uint32_t rgba8, int x0, int y0, int x1, int y1,
                             uint32_t lane, bool full) {
    SceneOp op;
    op.kind = SceneOp::CLEAR;
    op.clear_rgba = rgba8;
    op.clear_rect_full = full;
    op.clear_x0 = x0; op.clear_y0 = y0;
    op.clear_x1 = x1; op.clear_y1 = y1;
    op.clear_lane = lane;
    g_scene_ops.push_back(std::move(op));
}

// Called from glBitmap when it would otherwise just write to the live
// fb directly. The bitmap path bypasses the pipeline (fixed-function
// glRasterPos + glBitmap is a fb blit, not a draw call), so to keep
// the SC chain's fb in sync we record the same blit as an ordered op.
// Coords are FB-space lower-left (raster_pos minus xb/yb origin), the
// way glBitmap already computes them in glcompat_state.cpp.
void scene_record_bitmap(int x, int y, int w, int h,
                         uint32_t color_rgba8,
                         const uint8_t* bits, size_t byte_count) {
    SceneOp op;
    op.kind = SceneOp::BITMAP;
    op.bitmap.x = x; op.bitmap.y = y;
    op.bitmap.w = w; op.bitmap.h = h;
    op.bitmap.color = color_rgba8;
    op.bitmap.bits.assign(bits, bits + byte_count);
    g_scene_ops.push_back(std::move(op));
}
void scene_record_clear_depth(float v) {
    SceneOp op; op.kind = SceneOp::CLEAR_DEPTH; op.clear_depth = v;
    g_scene_ops.push_back(std::move(op));
}
void scene_record_clear_stencil(uint8_t v) {
    SceneOp op; op.kind = SceneOp::CLEAR_STENCIL; op.clear_stencil_val = v;
    g_scene_ops.push_back(std::move(op));
}

void flush_immediate() {
    auto& s = state();
    if (s.verts.empty()) return;
    if (std::getenv("GLCOMPAT_TRACE")) {
        std::fprintf(stderr,
            "[glcompat] flush prim=0x%x verts=%zu fb=%dx%d lighting=%d\n",
            s.prim, s.verts.size(),
            s.ctx.fb.width, s.ctx.fb.height, (int)s.lighting);
    }

    // Lazy framebuffer if user didn't glClear yet.
    if (!s.ctx_inited) {
        s.ctx.fb.width  = s.vp_w > 0 ? s.vp_w : 256;
        s.ctx.fb.height = s.vp_h > 0 ? s.vp_h : 256;
        s.ctx.fb.color.assign((size_t)s.ctx.fb.width * s.ctx.fb.height, 0u);
        s.ctx.fb.depth.assign((size_t)s.ctx.fb.width * s.ctx.fb.height, 1.0f);
        s.ctx_inited = true;
    }

    // Compose MVP.
    const Mat4 mv  = s.modelview.back();
    const Mat4 pj  = s.projection.back();
    const Mat4 mvp = mat4_mul(pj, mv);
    const Mat4 nrm_mat = s.lighting ? mat3_inv_transpose(mv) : mat4_identity();

    // Per-vertex transform → clip space, plus optional Gouraud lighting.
    struct LitVert { Vec4 clip; Vec4 color; };
    std::vector<LitVert> lit;
    lit.reserve(s.verts.size());
    for (const auto& iv : s.verts) {
        LitVert lv;
        lv.clip  = mat4_apply(mvp, iv.pos);
        lv.color = s.lighting ? gouraud(iv, mv, nrm_mat) : iv.color;
        lit.push_back(lv);
    }

    // Triangulate. For LINES/LINE_STRIP/LINE_LOOP, expand each segment
    // into a thin quad first.
    std::vector<std::array<int, 3>> tris;
    if (s.prim == GL_LINES || s.prim == GL_LINE_STRIP ||
        s.prim == GL_LINE_LOOP) {
        std::vector<Vec4> clip_v, col_v;
        clip_v.reserve(lit.size()); col_v.reserve(lit.size());
        for (const auto& lv : lit) { clip_v.push_back(lv.clip); col_v.push_back(lv.color); }
        auto x = expand_lines(s.prim, clip_v, col_v,
                              s.ctx.fb.width, s.ctx.fb.height);
        // Append to lit so subsequent draw uses them.
        for (size_t i = 0; i < x.extra_clip.size(); ++i)
            lit.push_back({x.extra_clip[i], x.extra_color[i]});
        tris = std::move(x.tris);
    } else {
        tris = triangulate(s.prim, (int)lit.size());
    }
    if (tris.empty()) return;

    // Build per-triangle vertex attribute stream for sw_ref.
    // Attrib slot 0 = position (clip space — pass through VS), slot 1 = colour.
    //
    // sw_ref's rasterizer only accepts triangles with positive screen-space
    // edge area (it doesn't actually back-face cull — it only renders one
    // winding). OpenGL convention is CCW = front in window coords with Y-up;
    // when the user sets up a Y-flipped viewport (e.g. simple.c), CCW in
    // window space becomes CW in NDC. Detect each triangle's projected
    // winding and swap vertices so the rasterizer always sees positive
    // area. Skipped if cull_face is enabled — the caller asked for one-sided.
    std::vector<gpu::Vec4f> positions;
    std::vector<gpu::Vec4f> colours;
    positions.reserve(tris.size() * 3);
    colours.reserve(tris.size() * 3);
    const bool capture_scene = std::getenv("GLCOMPAT_SCENE") != nullptr;
    // Pre-compute viewport-bake factors: when capturing a scene, the
    // SC chain has only one fb-wide viewport, so we collapse the
    // current viewport into clip-space here. Without this, multi-
    // window scenes would draw all sub-window content at the full fb
    // viewport on the SC side.
    const int FB_W = s.ctx.fb.width  > 0 ? s.ctx.fb.width  : 1;
    const int FB_H = s.ctx.fb.height > 0 ? s.ctx.fb.height : 1;
    const bool need_vp_bake = capture_scene
        && (s.vp_x != 0 || s.vp_y != 0
            || s.vp_w != FB_W || s.vp_h != FB_H);
    for (const auto& t : tris) {
        int i0 = t[0], i1 = t[1], i2 = t[2];
        if (!s.cull_face) {
            // Project to 2D NDC (y is then flipped by viewport, but
            // winding sign is preserved).
            const float w0 = lit[i0].clip[3], w1 = lit[i1].clip[3], w2 = lit[i2].clip[3];
            const float ax = lit[i0].clip[0] / (w0 != 0 ? w0 : 1);
            const float ay = lit[i0].clip[1] / (w0 != 0 ? w0 : 1);
            const float bx = lit[i1].clip[0] / (w1 != 0 ? w1 : 1);
            const float by = lit[i1].clip[1] / (w1 != 0 ? w1 : 1);
            const float cx = lit[i2].clip[0] / (w2 != 0 ? w2 : 1);
            const float cy = lit[i2].clip[1] / (w2 != 0 ? w2 : 1);
            const float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            // After viewport Y is mapped (ndc_y * 0.5 + 0.5) * h: positive
            // ndc-area becomes positive screen-area. So the sign here
            // matches the rasterizer's expectation.
            if (area < 0.0f) std::swap(i1, i2);
        }
        const std::array<int, 3> idx = {i0, i1, i2};
        for (int k = 0; k < 3; ++k) {
            const auto& lv = lit[idx[k]];
            gpu::Vec4f p{}; p[0]=lv.clip[0]; p[1]=lv.clip[1]; p[2]=lv.clip[2]; p[3]=lv.clip[3];
            gpu::Vec4f c{}; c[0]=lv.color[0]; c[1]=lv.color[1]; c[2]=lv.color[2]; c[3]=lv.color[3];
            positions.push_back(p);
            colours.push_back(c);
            if (capture_scene) {
                Vec4 cap = lv.clip;
                if (need_vp_bake) {
                    // Bake viewport: divide by w, scale/offset NDC into
                    // fb-NDC, store with w=1 so PA's persp-divide is a
                    // no-op on this side.
                    const float w = cap[3] != 0 ? cap[3] : 1;
                    const float ndc_x = cap[0] / w;
                    const float ndc_y = cap[1] / w;
                    const float ndc_z = cap[2] / w;
                    const float fbx = (s.vp_x + (ndc_x * 0.5f + 0.5f) * s.vp_w);
                    const float fby = (s.vp_y + (ndc_y * 0.5f + 0.5f) * s.vp_h);
                    cap[0] = 2.0f * fbx / FB_W - 1.0f;
                    cap[1] = 2.0f * fby / FB_H - 1.0f;
                    cap[2] = ndc_z;
                    cap[3] = 1.0f;
                }
                open_or_reuse_batch(s).verts.push_back({cap, lv.color});
            }
        }
    }

    // sw_ref Context: framebuffer already initialized; bind shaders + attribs.
    const auto& sp = shader_pair();
    s.ctx.shaders.vs_binary = &sp.vs;
    s.ctx.shaders.fs_binary = &sp.fs;
    s.ctx.shaders.vs_attr_count    = 2;
    s.ctx.shaders.vs_varying_count = 1;
    s.ctx.shaders.fs_varying_count = 1;
    s.ctx.draw.vp_x = s.vp_x;
    s.ctx.draw.vp_y = s.vp_y;
    s.ctx.draw.vp_w = s.vp_w;
    s.ctx.draw.vp_h = s.vp_h;
    s.ctx.draw.primitive = gpu::DrawState::TRIANGLES;
    s.ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                        sizeof(gpu::Vec4f), 0, positions.data()};
    s.ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                        sizeof(gpu::Vec4f), 0, colours.data()};

    gpu::pipeline::draw(s.ctx, (uint32_t)positions.size());
}

void save_framebuffer() {
    auto& s = state();
    if (!s.ctx_inited) return;
    const char* env = std::getenv("GLCOMPAT_OUT");
    const std::string path = env ? env : "out.ppm";
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    const int W = s.ctx.fb.width, H = s.ctx.fb.height;
    f << "P6\n" << W << " " << H << "\n255\n";
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const uint32_t c = s.ctx.fb.color[(size_t)y * W + x];
            f.put((char)((c >>  0) & 0xFF));
            f.put((char)((c >>  8) & 0xFF));
            f.put((char)((c >> 16) & 0xFF));
        }
    }
    std::fprintf(stderr, "[glcompat] saved %dx%d → %s\n", W, H, path.c_str());
}

namespace {
bool g_es2_capture = false;
}  // namespace

void set_es2_scene_capture(bool on) { g_es2_capture = on; }
bool es2_scene_capture_enabled() {
    return g_es2_capture || (std::getenv("GLCOMPAT_SCENE") != nullptr);
}

// Sprint 39 / Sprint 61: ES 2.0 batch capture. The runner has already
// walked the VBOs + run the VS in software, so all we do here is open
// (or reuse) a BATCH and append the (clip-space pos, varying[0..N-1])
// tuples. `varyings` is laid out per-vertex × N where each slot is a
// vec4. Sprint 61 — generalised from the single-varying form so the
// 1060-case `fragment_ops.blend.*` block can roundtrip multi-varying
// data through the SC chain replay.
void scene_record_es2_batch(const std::vector<gpu::Vec4f>& clip_pos,
                            const std::vector<std::array<gpu::Vec4f, 7>>& varyings,
                            int n_vars) {
    if (clip_pos.empty()) return;
    if (n_vars < 1) n_vars = 1;
    if (n_vars > 7) n_vars = 7;
    auto& batch = open_or_reuse_batch(state());
    if (batch.verts.empty())
        batch.n_vars = n_vars;
    else if (batch.n_vars < n_vars)
        batch.n_vars = n_vars;       // extend to widest seen so far
    const size_t n = std::min(clip_pos.size(), varyings.size());
    batch.verts.reserve(batch.verts.size() + n);
    for (size_t i = 0; i < n; ++i) {
        SceneVert sv;
        sv.pos[0] = clip_pos[i][0]; sv.pos[1] = clip_pos[i][1];
        sv.pos[2] = clip_pos[i][2]; sv.pos[3] = clip_pos[i][3];
        for (int k = 0; k < 7; ++k) {
            sv.vars[k][0] = varyings[i][k][0];
            sv.vars[k][1] = varyings[i][k][1];
            sv.vars[k][2] = varyings[i][k][2];
            sv.vars[k][3] = varyings[i][k][3];
        }
        batch.verts.push_back(sv);
    }
}

namespace {
void save_scene_impl(const char* path);
}

void save_scene_to(const std::string& path) {
    save_scene_impl(path.c_str());
}

void save_scene() {
    const char* env = std::getenv("GLCOMPAT_SCENE");
    if (!env) return;
    save_scene_impl(env);
}

// Sprint 59 — register an atexit hook the first time scene capture is
// observed live, so non-GLUT clients (notably `deqp-gles2`) get the same
// scene dump on exit that the GLUT path triggers in glutLeaveMainLoop.
// The flag avoids piling up multiple atexit() registrations across a
// long-running test suite.
namespace {
void atexit_save_scene() {
    save_scene();
}
}  // namespace
void install_scene_atexit_once() {
    static bool installed = false;
    if (installed) return;
    if (!es2_scene_capture_enabled()) return;
    std::atexit(atexit_save_scene);
    installed = true;
    std::fprintf(stderr, "[glcompat] scene atexit registered (GLCOMPAT_SCENE=%s)\n",
                 std::getenv("GLCOMPAT_SCENE"));
}

namespace {
void save_scene_impl(const char* env) {
    auto& s = state();
    // Drop any trailing partial triangle inside each batch (glBegin
    // can leave dangling vertices if the program is malformed) and
    // collapse runs of clears that are immediately overwritten by
    // another clear (only the last in a run mutates the fb anyway).
    for (auto& op : g_scene_ops) {
        if (op.kind == SceneOp::BATCH && op.batch.verts.size() % 3 != 0)
            op.batch.verts.resize(op.batch.verts.size()
                                  - (op.batch.verts.size() % 3));
    }
    {
        std::vector<SceneOp> kept;
        kept.reserve(g_scene_ops.size());
        for (auto& op : g_scene_ops) {
            if (op.kind == SceneOp::BATCH && op.batch.verts.empty()) continue;
            // Sprint 60 — only collapse consecutive CLEAR ops when BOTH
            // are full-fb / full-mask. A scissored or masked clear only
            // covers part of the fb; the prior clear still matters for
            // the area it doesn't touch. The pre-Sprint-60 collapse
            // (which always merged) silently dropped the background
            // clear in `color_clear.scissored_*` / `masked_*` and the
            // SC replay rendered inside-scissor only on top of an
            // empty (init-zero) fb.
            const bool both_clears = (op.kind == SceneOp::CLEAR
                                      && !kept.empty()
                                      && kept.back().kind == SceneOp::CLEAR);
            const bool full_pair = both_clears
                && op.clear_rect_full && op.clear_lane == 0xFFFFFFFFu
                && kept.back().clear_rect_full
                && kept.back().clear_lane == 0xFFFFFFFFu;
            if (full_pair) {
                kept.back().clear_rgba = op.clear_rgba;
                continue;
            }
            kept.push_back(std::move(op));
        }
        g_scene_ops.swap(kept);
    }
    if (!s.ctx_inited && g_scene_ops.empty()) {
        std::fprintf(stderr, "[glcompat] no draw activity for scene\n");
        return;
    }
    std::ofstream f(env);
    if (!f) return;
    const int W = s.ctx.fb.width, H = s.ctx.fb.height;
    f << "# generated by glcompat (Sprint 41 — ordered ops)\n";
    f << "width  " << W << "\n";
    f << "height " << H << "\n";
    f << "msaa   " << (s.ctx.fb.msaa_4x ? 1 : 0) << "\n";
    // Initial fb clear colour. Use the FIRST op's clear if it's a
    // clear, else the live final clear_color (legacy fallback).
    auto to_u8 = [](float v) {
        if (v <= 0.0f) return 0u;
        if (v >= 1.0f) return 255u;
        return (unsigned)(v * 255.0f + 0.5f);
    };
    const auto& cc = s.clear_color;
    const unsigned cr = to_u8(cc[0]), cg = to_u8(cc[1]),
                   cb = to_u8(cc[2]), ca = to_u8(cc[3]);
    char buf[32]; std::snprintf(buf, sizeof(buf),
        "%02x%02x%02x%02x", ca, cb, cg, cr);
    f << "clear  " << buf << "\n";
    size_t total_tris = 0, n_batches = 0, n_clears = 0, n_bitmaps = 0;
    for (const auto& op : g_scene_ops) {
        if (op.kind == SceneOp::CLEAR) {
            char cbuf[32]; std::snprintf(cbuf, sizeof(cbuf), "%08x", op.clear_rgba);
            // Sprint 60 — emit the extended form whenever the clear was
            // actually scoped (scissor on or non-trivial color mask) so
            // the SC replay can reproduce sw_ref's behaviour. The bare
            // form stays available for whole-fb / full-mask clears so
            // older tooling + scenes still parse.
            if (op.clear_rect_full && op.clear_lane == 0xFFFFFFFFu) {
                f << "clear_rect " << cbuf << "\n";
            } else {
                char lbuf[16]; std::snprintf(lbuf, sizeof(lbuf), "%08x", op.clear_lane);
                f << "clear_rect " << cbuf << " "
                  << op.clear_x0 << " " << op.clear_y0 << " "
                  << op.clear_x1 << " " << op.clear_y1 << " " << lbuf << "\n";
            }
            ++n_clears;
            continue;
        }
        if (op.kind == SceneOp::CLEAR_DEPTH) {
            f << "clear_depth " << op.clear_depth << "\n";
            continue;
        }
        if (op.kind == SceneOp::CLEAR_STENCIL) {
            f << "clear_stencil " << (int)op.clear_stencil_val << "\n";
            continue;
        }
        if (op.kind == SceneOp::BITMAP) {
            const auto& bm = op.bitmap;
            char cbuf[32]; std::snprintf(cbuf, sizeof(cbuf), "%08x", bm.color);
            f << "bitmap " << bm.x << " " << bm.y << " "
              << bm.w << " " << bm.h << " " << cbuf << " ";
            for (uint8_t byte : bm.bits) {
                char hbuf[4]; std::snprintf(hbuf, sizeof(hbuf), "%02x", byte);
                f << hbuf;
            }
            f << "\n";
            ++n_bitmaps;
            continue;
        }
        const auto& b = op.batch;
        f << "batch\n";
        f << "  depth_test  " << (b.depth_test  ? 1 : 0) << "\n";
        f << "  depth_write " << (b.depth_write ? 1 : 0) << "\n";
        f << "  depth_func  " << b.depth_func << "\n";
        f << "  cull_back   " << (b.cull_back   ? 1 : 0) << "\n";
        f << "  blend       " << (b.blend       ? 1 : 0) << "\n";
        // Sprint 61 — emit full blend state so the SC replay reproduces
        // glBlendFuncSeparate / glBlendEquationSeparate / glBlendColor.
        // Skip the lines when blend is off and everything is at default
        // to keep diff churn minimal on legacy scenes.
        const bool blend_default =
            std::strcmp(b.blend_src_rgb,   "src_alpha")           == 0 &&
            std::strcmp(b.blend_dst_rgb,   "one_minus_src_alpha") == 0 &&
            std::strcmp(b.blend_src_alpha, "src_alpha")           == 0 &&
            std::strcmp(b.blend_dst_alpha, "one_minus_src_alpha") == 0 &&
            std::strcmp(b.blend_eq_rgb,    "add")                 == 0 &&
            std::strcmp(b.blend_eq_alpha,  "add")                 == 0;
        // Sprint 61 — per-batch viewport so the SC chain renders into
        // the same sub-rect as sw_ref (and the E2E driver knows where
        // to crop the SC PPM for diff). Default is whole-fb; emit only
        // when the test set a non-default viewport.
        if (b.vp_w > 0 && (b.vp_x != 0 || b.vp_y != 0 ||
                            b.vp_w != W || b.vp_h != H)) {
            f << "  viewport " << b.vp_x << " " << b.vp_y
              << " " << b.vp_w << " " << b.vp_h << "\n";
        }
        if (b.blend || !blend_default) {
            f << "  blend_func " << b.blend_src_rgb << " " << b.blend_dst_rgb
              << " " << b.blend_src_alpha << " " << b.blend_dst_alpha << "\n";
            f << "  blend_eq " << b.blend_eq_rgb << " " << b.blend_eq_alpha << "\n";
            if (b.blend_color[0] != 0.0f || b.blend_color[1] != 0.0f ||
                b.blend_color[2] != 0.0f || b.blend_color[3] != 0.0f) {
                f << "  blend_color "
                  << b.blend_color[0] << " " << b.blend_color[1] << " "
                  << b.blend_color[2] << " " << b.blend_color[3] << "\n";
            }
        }
        if (b.stencil_test) {
            f << "  stencil_test 1\n";
            f << "  stencil_func " << b.stencil_func << " " << b.stencil_ref
              << " " << b.stencil_read_mask << "\n";
            f << "  stencil_op " << b.stencil_sfail << " "
              << b.stencil_dpfail << " " << b.stencil_dppass << "\n";
            f << "  stencil_write_mask " << b.stencil_write_mask << "\n";
        }
        // Sprint 61 — emit `varying_count N` (default 1 stays implicit
        // for back-compat with older scenes / runners that only know
        // the single-varying form). Per vertex: 4 (pos) + 4·N (varying)
        // floats. The legacy 4+4 layout is exactly the N=1 case.
        const int n_vars = std::max(1, std::min(7, b.n_vars));
        if (n_vars != 1) f << "  varying_count " << n_vars << "\n";
        f << "  verts\n";
        for (const auto& v : b.verts) {
            f << "  " << v.pos[0] << " " << v.pos[1]
              << " "  << v.pos[2] << " " << v.pos[3];
            for (int k = 0; k < n_vars; ++k) {
                f << "  " << v.vars[k][0] << " " << v.vars[k][1]
                  << " "  << v.vars[k][2] << " " << v.vars[k][3];
            }
            f << "\n";
        }
        f << "  end\n";
        f << "end_batch\n";
        total_tris += b.verts.size() / 3;
        ++n_batches;
    }
    std::fprintf(stderr,
        "[glcompat] scene → %s (%zu triangles in %zu batches, %zu clears, %zu bitmaps)\n",
        env, total_tris, n_batches, n_clears, n_bitmaps);
}
}  // namespace (save_scene_impl)

}  // namespace glcompat
