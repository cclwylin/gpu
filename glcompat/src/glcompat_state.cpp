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
void glDrawPixels(GLsizei, GLsizei, GLenum, GLenum, const GLvoid*) {}
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*) {}
void glRasterPos2i(GLint, GLint)          {}
void glRasterPos2f(GLfloat, GLfloat)      {}
void glRasterPos3f(GLfloat, GLfloat, GLfloat) {}
void glBitmap(GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte*) {}
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

// Eval / map — full stubs.
void glMap1d(GLenum, GLdouble, GLdouble, GLint, GLint, const GLdouble*) {}
void glMap1f(GLenum, GLfloat,  GLfloat,  GLint, GLint, const GLfloat*)  {}
void glMap2d(GLenum, GLdouble, GLdouble, GLint, GLint,
             GLdouble, GLdouble, GLint, GLint, const GLdouble*) {}
void glMap2f(GLenum, GLfloat,  GLfloat,  GLint, GLint,
             GLfloat,  GLfloat,  GLint, GLint, const GLfloat*)  {}
void glMapGrid1d(GLint, GLdouble, GLdouble) {}
void glMapGrid2d(GLint, GLdouble, GLdouble, GLint, GLdouble, GLdouble) {}
void glMapGrid2f(GLint, GLfloat,  GLfloat,  GLint, GLfloat,  GLfloat)  {}
void glEvalCoord1d(GLdouble) {}
void glEvalCoord2d(GLdouble, GLdouble) {}
void glEvalMesh1(GLenum, GLint, GLint) {}
void glEvalMesh2(GLenum, GLint, GLint, GLint, GLint) {}
void glEvalPoint1(GLint) {}
void glEvalPoint2(GLint, GLint) {}
void glPixelZoom(GLfloat, GLfloat) {}
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

}   // extern "C"
