// Sprint 36/37/38 — glEnd → render via gpu::pipeline (sw_ref).
//
// Takes the buffered immediate-mode vertices, applies MVP, optionally
// runs Gouraud lighting, triangulates per the begin-mode, and submits
// to gpu::pipeline::draw with auto-generated VS/FS bytecode.

#include "glcompat_runtime.h"

#include <GL/gl.h>

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
        default:
            break;
    }
    return tris;
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

// Accumulated triangle list captured for the optional .scene dump.
namespace { struct SceneVert { Vec4 pos; Vec4 col; };
            std::vector<SceneVert> g_scene_buf;
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

    // Triangulate and render.
    const auto tris = triangulate(s.prim, (int)lit.size());
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
            if (capture_scene)
                g_scene_buf.push_back({lv.clip, lv.color});
        }
    }

    // sw_ref Context: framebuffer already initialized; bind shaders + attribs.
    const auto& sp = shader_pair();
    s.ctx.shaders.vs_binary = &sp.vs;
    s.ctx.shaders.fs_binary = &sp.fs;
    s.ctx.shaders.vs_attr_count    = 2;
    s.ctx.shaders.vs_varying_count = 1;
    s.ctx.shaders.fs_varying_count = 1;
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

void save_scene() {
    const char* env = std::getenv("GLCOMPAT_SCENE");
    if (!env) return;
    auto& s = state();
    if (g_scene_buf.empty()) {
        std::fprintf(stderr, "[glcompat] no geometry captured for scene\n");
        return;
    }
    if (g_scene_buf.size() % 3 != 0) {
        // Trim trailing partial — scene format wants triplets only.
        g_scene_buf.resize(g_scene_buf.size() - (g_scene_buf.size() % 3));
    }
    std::ofstream f(env);
    if (!f) return;
    const int W = s.ctx.fb.width, H = s.ctx.fb.height;
    f << "# generated by glcompat (Sprint 39)\n";
    f << "width  " << W << "\n";
    f << "height " << H << "\n";
    f << "msaa   " << (s.ctx.fb.msaa_4x ? 1 : 0) << "\n";
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
    f << "verts\n";
    for (const auto& v : g_scene_buf) {
        f << " " << v.pos[0] << " " << v.pos[1]
          << " " << v.pos[2] << " " << v.pos[3]
          << "  " << v.col[0] << " " << v.col[1]
          << " "  << v.col[2] << " " << v.col[3] << "\n";
    }
    f << "end\n";
    std::fprintf(stderr,
        "[glcompat] scene → %s (%zu triangles)\n",
        env, g_scene_buf.size() / 3);
}

}  // namespace glcompat
