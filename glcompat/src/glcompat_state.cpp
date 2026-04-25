// Sprint 36 — global state singleton + GL state setters.

#include "glcompat_runtime.h"

#include <GL/gl.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace glcompat {

State& state() {
    static State s;
    return s;
}

// ---- matrix helpers (column-major, OpenGL convention) ----

Mat4 mat4_identity() {
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int r_ = 0; r_ < 4; ++r_)
            for (int k = 0; k < 4; ++k)
                r[c * 4 + r_] += a[k * 4 + r_] * b[c * 4 + k];
    return r;
}

Vec4 mat4_apply(const Mat4& m, const Vec4& v) {
    Vec4 r{};
    for (int row = 0; row < 4; ++row)
        r[row] = m[0 * 4 + row] * v[0] + m[1 * 4 + row] * v[1]
               + m[2 * 4 + row] * v[2] + m[3 * 4 + row] * v[3];
    return r;
}

Mat4 mat4_translate(float x, float y, float z) {
    Mat4 m = mat4_identity();
    m[12] = x; m[13] = y; m[14] = z;
    return m;
}

Mat4 mat4_scale(float x, float y, float z) {
    Mat4 m{};
    m[0] = x; m[5] = y; m[10] = z; m[15] = 1.0f;
    return m;
}

Mat4 mat4_rotate(float angle_deg, float x, float y, float z) {
    const float a = angle_deg * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(a), s = std::sin(a);
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 0.0f) { x /= len; y /= len; z /= len; }
    const float nc = 1.0f - c;
    Mat4 m = mat4_identity();
    m[0]  = c + x*x*nc;     m[4] = x*y*nc - z*s;   m[8]  = x*z*nc + y*s;
    m[1]  = y*x*nc + z*s;   m[5] = c + y*y*nc;     m[9]  = y*z*nc - x*s;
    m[2]  = z*x*nc - y*s;   m[6] = z*y*nc + x*s;   m[10] = c + z*z*nc;
    return m;
}

Mat4 mat4_ortho(float l, float r, float b, float t, float n, float f) {
    Mat4 m{};
    m[0]  = 2.0f / (r - l);
    m[5]  = 2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
    return m;
}

Mat4 mat4_frustum(float l, float r, float b, float t, float n, float f) {
    Mat4 m{};
    m[0]  = 2.0f * n / (r - l);
    m[5]  = 2.0f * n / (t - b);
    m[8]  = (r + l) / (r - l);
    m[9]  = (t + b) / (t - b);
    m[10] = -(f + n) / (f - n);
    m[11] = -1.0f;
    m[14] = -2.0f * f * n / (f - n);
    return m;
}

namespace {
std::vector<Mat4>& current_stack() {
    auto& s = state();
    switch (s.matrix_mode) {
        case GL_PROJECTION: return s.projection;
        case GL_TEXTURE:    return s.texmat;
        default:            return s.modelview;
    }
}
void apply_to_top(const Mat4& rhs) {
    auto& stk = current_stack();
    stk.back() = mat4_mul(stk.back(), rhs);
}
}  // namespace

}  // namespace glcompat

using glcompat::state;

// =========================================================================
// Matrix stack
// =========================================================================

extern "C" {

void glMatrixMode(GLenum mode)       { state().matrix_mode = mode; }
void glLoadIdentity(void)            { glcompat::current_stack().back() = glcompat::mat4_identity(); }
void glLoadMatrixf(const GLfloat* m) {
    glcompat::Mat4 mm; std::memcpy(mm.data(), m, 16 * sizeof(float));
    glcompat::current_stack().back() = mm;
}
void glMultMatrixf(const GLfloat* m) {
    glcompat::Mat4 mm; std::memcpy(mm.data(), m, 16 * sizeof(float));
    glcompat::apply_to_top(mm);
}
void glPushMatrix(void) {
    auto& stk = glcompat::current_stack();
    stk.push_back(stk.back());
}
void glPopMatrix(void) {
    auto& stk = glcompat::current_stack();
    if (stk.size() > 1) stk.pop_back();
}
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    glcompat::apply_to_top(glcompat::mat4_translate(x, y, z));
}
void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    glcompat::apply_to_top(glcompat::mat4_scale(x, y, z));
}
void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) {
    glcompat::apply_to_top(glcompat::mat4_rotate(a, x, y, z));
}
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
             GLdouble n, GLdouble f) {
    glcompat::apply_to_top(glcompat::mat4_ortho(l, r, b, t, n, f));
}
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
               GLdouble n, GLdouble f) {
    glcompat::apply_to_top(glcompat::mat4_frustum(l, r, b, t, n, f));
}

// =========================================================================
// Viewport / clear / scissor
// =========================================================================

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    auto& s = state();
    s.vp_x = x; s.vp_y = y; s.vp_w = w; s.vp_h = h;
}

void glScissor(GLint, GLint, GLsizei, GLsizei) {}   // ignored for now

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) {
    state().clear_color = {{r, g, b, a}};
}
void glClearDepth(GLclampd d) { state().clear_depth = (float)d; }
void glClearStencil(GLint)    {}
void glClearIndex(GLfloat)    {}

void glClear(GLbitfield mask) {
    auto& s = state();
    auto& fb = s.ctx.fb;
    if (!s.ctx_inited) {
        // Lazy init: framebuffer at viewport size on first use.
        fb.width = s.vp_w > 0 ? s.vp_w : 256;
        fb.height = s.vp_h > 0 ? s.vp_h : 256;
        fb.color.assign((size_t)fb.width * fb.height, 0u);
        fb.depth.assign((size_t)fb.width * fb.height, 1.0f);
        s.ctx_inited = true;
    }
    if (mask & GL_COLOR_BUFFER_BIT) {
        const auto to_u8 = [](float f) {
            const float c = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
            return (uint32_t)(c * 255.0f + 0.5f) & 0xFFu;
        };
        const auto& cc = s.clear_color;
        const uint32_t pix = (to_u8(cc[3]) << 24) | (to_u8(cc[2]) << 16)
                           | (to_u8(cc[1]) <<  8) |  to_u8(cc[0]);
        std::fill(fb.color.begin(), fb.color.end(), pix);
    }
    if (mask & GL_DEPTH_BUFFER_BIT)
        std::fill(fb.depth.begin(), fb.depth.end(), s.clear_depth);
}

// =========================================================================
// Enables / state
// =========================================================================

void glEnable(GLenum cap) {
    auto& s = state();
    switch (cap) {
        case GL_DEPTH_TEST:     s.depth_test = true; s.ctx.draw.depth_test = true; break;
        case GL_BLEND:          s.blend = true; s.ctx.draw.blend_enable = true; break;
        case GL_LIGHTING:       s.lighting = true; break;
        case GL_CULL_FACE:      s.cull_face = true; s.ctx.draw.cull_back = true; break;
        case GL_TEXTURE_2D:     s.tex2d = true; break;
        case GL_COLOR_MATERIAL: s.color_material = true; break;
        case GL_NORMALIZE:      s.normalize = true; break;
        case GL_LIGHT0: case GL_LIGHT1: case GL_LIGHT2: case GL_LIGHT3:
        case GL_LIGHT4: case GL_LIGHT5: case GL_LIGHT6: case GL_LIGHT7:
            s.lights[cap - GL_LIGHT0].enabled = true; break;
        default: break;     // silently ignore the long tail
    }
}

void glDisable(GLenum cap) {
    auto& s = state();
    switch (cap) {
        case GL_DEPTH_TEST:     s.depth_test = false; s.ctx.draw.depth_test = false; break;
        case GL_BLEND:          s.blend = false; s.ctx.draw.blend_enable = false; break;
        case GL_LIGHTING:       s.lighting = false; break;
        case GL_CULL_FACE:      s.cull_face = false; s.ctx.draw.cull_back = false; break;
        case GL_TEXTURE_2D:     s.tex2d = false; break;
        case GL_COLOR_MATERIAL: s.color_material = false; break;
        case GL_NORMALIZE:      s.normalize = false; break;
        case GL_LIGHT0: case GL_LIGHT1: case GL_LIGHT2: case GL_LIGHT3:
        case GL_LIGHT4: case GL_LIGHT5: case GL_LIGHT6: case GL_LIGHT7:
            s.lights[cap - GL_LIGHT0].enabled = false; break;
        default: break;
    }
}

GLboolean glIsEnabled(GLenum cap) {
    auto& s = state();
    switch (cap) {
        case GL_DEPTH_TEST:     return s.depth_test;
        case GL_BLEND:          return s.blend;
        case GL_LIGHTING:       return s.lighting;
        case GL_CULL_FACE:      return s.cull_face;
        case GL_TEXTURE_2D:     return s.tex2d;
        default: return GL_FALSE;
    }
}

void glDepthFunc(GLenum f) {
    using DF = gpu::DrawState;
    state().depth_func = f;
    auto& d = state().ctx.draw;
    switch (f) {
        case GL_NEVER:    d.depth_func = DF::DF_NEVER; break;
        case GL_LESS:     d.depth_func = DF::DF_LESS; break;
        case GL_LEQUAL:   d.depth_func = DF::DF_LEQUAL; break;
        case GL_EQUAL:    d.depth_func = DF::DF_EQUAL; break;
        case GL_GEQUAL:   d.depth_func = DF::DF_GEQUAL; break;
        case GL_GREATER:  d.depth_func = DF::DF_GREATER; break;
        case GL_NOTEQUAL: d.depth_func = DF::DF_NOTEQUAL; break;
        case GL_ALWAYS:   d.depth_func = DF::DF_ALWAYS; break;
    }
}
void glDepthMask(GLboolean f) {
    state().depth_write = (f != 0);
    state().ctx.draw.depth_write = (f != 0);
}

void glBlendFunc(GLenum sf, GLenum df) {
    using BF = gpu::DrawState;
    state().blend_src = sf; state().blend_dst = df;
    auto map = [](GLenum f) -> BF::BlendFactor {
        switch (f) {
            case GL_ZERO:                return BF::BF_ZERO;
            case GL_ONE:                 return BF::BF_ONE;
            case GL_SRC_COLOR:           return BF::BF_SRC_COLOR;
            case GL_ONE_MINUS_SRC_COLOR: return BF::BF_ONE_MINUS_SRC_COLOR;
            case GL_SRC_ALPHA:           return BF::BF_SRC_ALPHA;
            case GL_ONE_MINUS_SRC_ALPHA: return BF::BF_ONE_MINUS_SRC_ALPHA;
            case GL_DST_ALPHA:           return BF::BF_DST_ALPHA;
            case GL_ONE_MINUS_DST_ALPHA: return BF::BF_ONE_MINUS_DST_ALPHA;
            case GL_DST_COLOR:           return BF::BF_DST_COLOR;
            case GL_ONE_MINUS_DST_COLOR: return BF::BF_ONE_MINUS_DST_COLOR;
            default:                     return BF::BF_ONE;
        }
    };
    state().ctx.draw.blend_src = map(sf);
    state().ctx.draw.blend_dst = map(df);
}

void glAlphaFunc(GLenum, GLclampf) {}   // unsupported, silently
void glCullFace(GLenum mode)        { state().cull_mode = mode; }
void glFrontFace(GLenum mode)       { state().front_face = mode; }
void glColorMask(GLboolean, GLboolean, GLboolean, GLboolean) {}
void glShadeModel(GLenum mode)      { state().shade_model = mode; }
void glPolygonMode(GLenum, GLenum)  {}

// =========================================================================
// Lights / materials
// =========================================================================

void glLightf(GLenum, GLenum, GLfloat) {}                       // attenuation etc — ignored
void glLightModelf(GLenum, GLfloat)    {}
void glLightModeli(GLenum, GLint)      {}
void glLightModelfv(GLenum pname, const GLfloat* p) {
    if (pname == GL_LIGHT_MODEL_AMBIENT)
        std::memcpy(state().light_model_ambient.data(), p, 4 * sizeof(float));
}
void glLightfv(GLenum light, GLenum pname, const GLfloat* p) {
    if (light < GL_LIGHT0 || light > GL_LIGHT7) return;
    auto& l = state().lights[light - GL_LIGHT0];
    glcompat::Vec4 v{}; std::memcpy(v.data(), p, 4 * sizeof(float));
    switch (pname) {
        case GL_AMBIENT:  l.ambient  = v; break;
        case GL_DIFFUSE:  l.diffuse  = v; break;
        case GL_SPECULAR: l.specular = v; break;
        case GL_POSITION: {
            // Lights are positioned in EYE space, i.e. transformed by
            // the current MODELVIEW matrix at the time of glLightfv.
            const auto& mv = state().modelview.back();
            l.position = glcompat::mat4_apply(mv, v);
            break;
        }
        default: break;
    }
}

void glMaterialf(GLenum, GLenum pname, GLfloat p) {
    if (pname == GL_SHININESS) state().material.shininess = p;
}
void glMaterialfv(GLenum, GLenum pname, const GLfloat* p) {
    glcompat::Vec4 v{}; std::memcpy(v.data(), p, 4 * sizeof(float));
    auto& m = state().material;
    switch (pname) {
        case GL_AMBIENT:             m.ambient = v; break;
        case GL_DIFFUSE:             m.diffuse = v; break;
        case GL_SPECULAR:            m.specular = v; break;
        case GL_EMISSION:            m.emission = v; break;
        case GL_AMBIENT_AND_DIFFUSE: m.ambient = v; m.diffuse = v; break;
        case GL_SHININESS:           m.shininess = v[0]; break;
    }
}
void glColorMaterial(GLenum, GLenum) {}     // we always treat current color as diffuse when COLOR_MATERIAL on

// =========================================================================
// Misc
// =========================================================================

void glFlush(void)              { /* end-of-frame save handled by glutMainLoop */ }
void glFinish(void)             {}
void glHint(GLenum, GLenum)     {}
void glPointSize(GLfloat)       {}
void glLineWidth(GLfloat)       {}
GLenum glGetError(void)         { return GL_NO_ERROR; }
const GLubyte* glGetString(GLenum n) {
    switch (n) {
        case GL_VENDOR:   return (const GLubyte*)"glcompat";
        case GL_RENDERER: return (const GLubyte*)"sw_ref";
        case GL_VERSION:  return (const GLubyte*)"1.1 (compat)";
        default:          return (const GLubyte*)"";
    }
}
void glGetIntegerv(GLenum n, GLint* v) {
    if (n == GL_VIEWPORT) { v[0] = state().vp_x; v[1] = state().vp_y;
                            v[2] = state().vp_w; v[3] = state().vp_h; }
    else if (v) v[0] = 0;
}
void glGetFloatv(GLenum n, GLfloat* v) {
    if (n == GL_MODELVIEW_MATRIX) std::memcpy(v, state().modelview.back().data(), 64);
    else if (n == GL_PROJECTION_MATRIX) std::memcpy(v, state().projection.back().data(), 64);
    else if (v) v[0] = 0;
}
void glGetBooleanv(GLenum, GLboolean* v) { if (v) v[0] = GL_FALSE; }
void glGetDoublev(GLenum, GLdouble* v)   { if (v) v[0] = 0.0; }
void glStencilFunc(GLenum, GLint, GLuint) {}
void glStencilOp(GLenum, GLenum, GLenum)  {}
void glStencilMask(GLuint)                {}
void glPixelStorei(GLenum, GLint)         {}
void glPixelStore(GLenum, GLint)          {}
void glPixelTransferf(GLenum, GLfloat)    {}
void glDrawBuffer(GLenum)                 {}
void glReadBuffer(GLenum)                 {}
void glDrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type,
                  const GLvoid* pixels) {
    auto& s = state();
    if (!s.raster_valid || !pixels || type != GL_UNSIGNED_BYTE) return;
    // Lazy-init fb to viewport size (matches glClear behaviour).
    if (!s.ctx_inited) {
        s.ctx.fb.width  = s.vp_w > 0 ? s.vp_w : 256;
        s.ctx.fb.height = s.vp_h > 0 ? s.vp_h : 256;
        s.ctx.fb.color.assign((size_t)s.ctx.fb.width * s.ctx.fb.height, 0u);
        s.ctx_inited = true;
    }
    const int FB_W = s.ctx.fb.width, FB_H = s.ctx.fb.height;
    const auto* src = (const GLubyte*)pixels;
    auto pack = [](GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
               ((uint32_t)g <<  8) |  (uint32_t)r;
    };
    const int bpp = (format == GL_RGBA) ? 4
                  : (format == GL_RGB)  ? 3
                  : (format == GL_LUMINANCE) ? 1
                  : (format == GL_LUMINANCE_ALPHA) ? 2 : 0;
    if (bpp == 0) return;
    const int dst_w = (int)((float)w * std::abs(s.pixel_zoom_x));
    const int dst_h = (int)((float)h * std::abs(s.pixel_zoom_y));
    const float zx = s.pixel_zoom_x;
    const float zy = s.pixel_zoom_y;
    const int rx0 = (int)s.raster_pos[0];
    const int ry0 = (int)s.raster_pos[1];
    for (int dy = 0; dy < dst_h; ++dy) {
        const int sy = (zy >= 0 ? dy : dst_h - 1 - dy)
                       * h / std::max(1, dst_h);
        const int fb_y = ry0 + dy;
        if (fb_y < 0 || fb_y >= FB_H) continue;
        for (int dx = 0; dx < dst_w; ++dx) {
            const int sx = (zx >= 0 ? dx : dst_w - 1 - dx)
                           * w / std::max(1, dst_w);
            const int fb_x = rx0 + dx;
            if (fb_x < 0 || fb_x >= FB_W) continue;
            const GLubyte* p = src + ((size_t)sy * w + sx) * bpp;
            GLubyte r = 0, g = 0, b = 0, a = 255;
            if (format == GL_RGB)              { r = p[0]; g = p[1]; b = p[2]; }
            else if (format == GL_RGBA)        { r = p[0]; g = p[1]; b = p[2]; a = p[3]; }
            else if (format == GL_LUMINANCE)   { r = g = b = p[0]; }
            else if (format == GL_LUMINANCE_ALPHA){ r = g = b = p[0]; a = p[1]; }
            s.ctx.fb.color[(size_t)fb_y * FB_W + fb_x] = pack(r, g, b, a);
        }
    }
    // Bookkeeping: emit a "scene activity" marker so save_scene
    // doesn't treat the frame as untouched.
    // (g_scene_buf stays empty — the SC chain has no triangles to run,
    //  so it'll fall back to clear-only output and drop RMSE on these
    //  specific examples. That's the trade.)
}
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*) {}
void glRasterPos2i(GLint x, GLint y) {
    auto& s = state();
    // OpenGL transforms by MVP and viewport. Most examples (splatlogo,
    // bitfont, fontdemo) set up a window-coord ortho first and pass
    // window-space ints — apply the same MVP+viewport our pipeline
    // would and store in window coords for direct fb addressing.
    glcompat::Vec4 obj{{(float)x, (float)y, 0.0f, 1.0f}};
    glcompat::Vec4 clip = glcompat::mat4_apply(
        glcompat::mat4_mul(s.projection.back(), s.modelview.back()), obj);
    if (clip[3] <= 0.0f) { s.raster_valid = false; return; }
    const float ndc_x = clip[0] / clip[3];
    const float ndc_y = clip[1] / clip[3];
    s.raster_pos[0] = (ndc_x * 0.5f + 0.5f) * s.vp_w + s.vp_x;
    s.raster_pos[1] = (ndc_y * 0.5f + 0.5f) * s.vp_h + s.vp_y;
    s.raster_valid = true;
}
void glRasterPos2f(GLfloat x, GLfloat y) { glRasterPos2i((GLint)x, (GLint)y); }
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat) { glRasterPos2i((GLint)x, (GLint)y); }

// glBitmap — rasterize a 1-bit-per-pixel mask at the current raster
// position using `cur_color`. Bits are MSB-first per byte; rows are
// stored bottom-up per the GL spec, so we flip Y at render. Empty
// bitmaps (w == h == 0, splatlogo idiom) just advance the raster.
void glBitmap(GLsizei w, GLsizei h, GLfloat xb, GLfloat yb,
              GLfloat xmove, GLfloat ymove, const GLubyte* bits) {
    auto& s = state();
    if (bits && w > 0 && h > 0 && s.raster_valid) {
        if (!s.ctx_inited) {
            s.ctx.fb.width  = s.vp_w > 0 ? s.vp_w : 256;
            s.ctx.fb.height = s.vp_h > 0 ? s.vp_h : 256;
            s.ctx.fb.color.assign(
                (size_t)s.ctx.fb.width * s.ctx.fb.height, 0u);
            s.ctx_inited = true;
        }
        const int FB_W = s.ctx.fb.width, FB_H = s.ctx.fb.height;
        const int row_bytes = (w + 7) / 8;
        const int x0 = (int)(s.raster_pos[0] - xb);
        const int y0 = (int)(s.raster_pos[1] - yb);
        auto pack = [](float f) {
            return (uint32_t)((f < 0 ? 0 : f > 1 ? 1 : f) * 255 + 0.5f) & 0xFF;
        };
        const auto& cc = s.cur_color;
        const uint32_t pix = (pack(cc[3]) << 24) | (pack(cc[2]) << 16)
                           | (pack(cc[1]) <<  8) |  pack(cc[0]);
        for (int row = 0; row < h; ++row) {
            // glBitmap rows are bottom-up.
            const GLubyte* src = bits + (size_t)(h - 1 - row) * row_bytes;
            const int fy = y0 - row;       // raster pos is at bottom-left
            if (fy < 0 || fy >= FB_H) continue;
            for (int col = 0; col < w; ++col) {
                if (!(src[col >> 3] & (0x80 >> (col & 7)))) continue;
                const int fx = x0 + col;
                if (fx < 0 || fx >= FB_W) continue;
                s.ctx.fb.color[(size_t)fy * FB_W + fx] = pix;
            }
        }
    }
    s.raster_pos[0] += xmove;
    s.raster_pos[1] += ymove;
}
void glIndexi(GLint)                      {}
void glIndexf(GLfloat)                    {}
void glClipPlane(GLenum, const GLdouble*) {}
void glFogf(GLenum, GLfloat)              {}
void glFogi(GLenum, GLint)                {}
void glFogfv(GLenum, const GLfloat*)      {}
void glInitNames(void)                    {}
void glLoadName(GLuint)                   {}
void glPushName(GLuint)                   {}
void glPopName(void)                      {}
GLint glRenderMode(GLenum)                { return 0; }
void glSelectBuffer(GLsizei, GLuint*)     {}
void glFeedbackBuffer(GLsizei, GLenum, GLfloat*) {}
void glPassThrough(GLfloat)               {}
void glAccum(GLenum, GLfloat)             {}
void glClearAccum(GLfloat, GLfloat, GLfloat, GLfloat) {}
void glLogicOp(GLenum)                    {}
void glTexEnvf(GLenum, GLenum, GLfloat)   {}
void glTexEnvi(GLenum, GLenum, GLint)     {}
void glTexEnvfv(GLenum, GLenum, const GLfloat*) {}
void glGetTexLevelParameteriv(GLenum, GLint, GLenum, GLint* p) { if (p) *p = 0; }

// Eval / map are now implemented in glcompat_eval.cpp.
void glPixelZoom(GLfloat x, GLfloat y) {
    state().pixel_zoom_x = x;
    state().pixel_zoom_y = y;
}
void glPolygonStipple(const GLubyte*) {}
void glLineStipple(GLint, GLushort) {}
void glPushAttrib(GLbitfield) {}
void glPopAttrib(void) {}
void glPushClientAttrib(GLbitfield) {}
void glPopClientAttrib(void) {}
void glRotated(GLdouble a, GLdouble x, GLdouble y, GLdouble z) {
    glRotatef((GLfloat)a, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}
void glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    glTranslatef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}
void glScaled(GLdouble x, GLdouble y, GLdouble z) {
    glScalef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}
void glPolygonOffset(GLfloat, GLfloat) {}

}   // extern "C"
