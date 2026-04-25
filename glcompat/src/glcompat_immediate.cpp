// Sprint 36 — immediate mode (glBegin..glEnd) + display lists.

#include "glcompat_runtime.h"

#include <GL/gl.h>

#include <cmath>
#include <cstring>

using glcompat::state;
using glcompat::ImmVertex;

namespace glcompat {

void record_or_apply_color(const Vec4& c) {
    auto& s = state();
    if (s.in_dlist) {
        DListCmd cmd; cmd.kind = DListCmd::COLOR; cmd.v = c;
        s.dlists[s.dlist_writing].push_back(cmd);
        if (s.dlist_mode != GL_COMPILE_AND_EXECUTE) return;
    }
    s.cur_color = c;
}
void record_or_apply_normal(const Vec3& n) {
    auto& s = state();
    if (s.in_dlist) {
        DListCmd cmd; cmd.kind = DListCmd::NORMAL; cmd.n = n;
        s.dlists[s.dlist_writing].push_back(cmd);
        if (s.dlist_mode != GL_COMPILE_AND_EXECUTE) return;
    }
    s.cur_normal = n;
}
void record_or_apply_texcoord(const Vec4& t) {
    auto& s = state();
    if (s.in_dlist) {
        DListCmd cmd; cmd.kind = DListCmd::TEXCOORD; cmd.v = t;
        s.dlists[s.dlist_writing].push_back(cmd);
        if (s.dlist_mode != GL_COMPILE_AND_EXECUTE) return;
    }
    s.cur_tex = t;
}

void record_or_apply_vertex(const Vec4& p) {
    auto& s = state();
    if (s.in_dlist) {
        DListCmd cmd; cmd.kind = DListCmd::VERTEX; cmd.v = p;
        s.dlists[s.dlist_writing].push_back(cmd);
        if (s.dlist_mode != GL_COMPILE_AND_EXECUTE) return;
    }
    if (!s.in_begin) return;        // glVertex outside glBegin → drop
    ImmVertex iv;
    iv.pos    = p;
    iv.color  = s.cur_color;
    iv.normal = s.cur_normal;
    iv.tex    = s.cur_tex;
    s.verts.push_back(iv);
}

void record_or_apply_begin(GLenum mode) {
    auto& s = state();
    if (s.in_dlist) {
        DListCmd cmd; cmd.kind = DListCmd::BEGIN; cmd.mode = mode;
        s.dlists[s.dlist_writing].push_back(cmd);
        if (s.dlist_mode != GL_COMPILE_AND_EXECUTE) return;
    }
    s.in_begin = true;
    s.prim     = mode;
    s.verts.clear();
}

void record_or_apply_end() {
    auto& s = state();
    if (s.in_dlist) {
        DListCmd cmd; cmd.kind = DListCmd::END;
        s.dlists[s.dlist_writing].push_back(cmd);
        if (s.dlist_mode != GL_COMPILE_AND_EXECUTE) return;
    }
    if (!s.in_begin) return;
    s.in_begin = false;
    flush_immediate();
    s.verts.clear();
}

}  // namespace glcompat

extern "C" {

void glBegin(GLenum mode) { glcompat::record_or_apply_begin(mode); }
void glEnd(void)          { glcompat::record_or_apply_end(); }

void glVertex2i(GLint x, GLint y) {
    glcompat::record_or_apply_vertex({{(float)x, (float)y, 0.0f, 1.0f}});
}
void glVertex2f(GLfloat x, GLfloat y) {
    glcompat::record_or_apply_vertex({{x, y, 0.0f, 1.0f}});
}
void glVertex2fv(const GLfloat* v) {
    glcompat::record_or_apply_vertex({{v[0], v[1], 0.0f, 1.0f}});
}
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    glcompat::record_or_apply_vertex({{x, y, z, 1.0f}});
}
void glVertex3fv(const GLfloat* v) {
    glcompat::record_or_apply_vertex({{v[0], v[1], v[2], 1.0f}});
}
void glVertex3d(GLdouble x, GLdouble y, GLdouble z) {
    glcompat::record_or_apply_vertex({{(float)x, (float)y, (float)z, 1.0f}});
}
void glVertex3dv(const GLdouble* v) {
    glcompat::record_or_apply_vertex(
        {{(float)v[0], (float)v[1], (float)v[2], 1.0f}});
}
void glVertex3i(GLint x, GLint y, GLint z) {
    glcompat::record_or_apply_vertex({{(float)x, (float)y, (float)z, 1.0f}});
}
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    glcompat::record_or_apply_vertex({{x, y, z, w}});
}
void glVertex4fv(const GLfloat* v) {
    glcompat::record_or_apply_vertex({{v[0], v[1], v[2], v[3]}});
}
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    glcompat::record_or_apply_color(
        {{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f}});
}
void glColor4ubv(const GLubyte* v) {
    glcompat::record_or_apply_color(
        {{v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f}});
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    glcompat::record_or_apply_color({{r, g, b, 1.0f}});
}
void glColor3fv(const GLfloat* v) {
    glcompat::record_or_apply_color({{v[0], v[1], v[2], 1.0f}});
}
void glColor3ub(GLubyte r, GLubyte g, GLubyte b) {
    glcompat::record_or_apply_color({{r / 255.0f, g / 255.0f, b / 255.0f, 1.0f}});
}
void glColor3ubv(const GLubyte* v) {
    glcompat::record_or_apply_color(
        {{v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, 1.0f}});
}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    glcompat::record_or_apply_color({{r, g, b, a}});
}
void glColor4fv(const GLfloat* v) {
    glcompat::record_or_apply_color({{v[0], v[1], v[2], v[3]}});
}

void glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    glcompat::record_or_apply_normal({{x, y, z}});
}
void glNormal3fv(const GLfloat* v) {
    glcompat::record_or_apply_normal({{v[0], v[1], v[2]}});
}

void glTexCoord1f(GLfloat s) {
    glcompat::record_or_apply_texcoord({{s, 0.0f, 0.0f, 1.0f}});
}
void glTexCoord2f(GLfloat s, GLfloat t) {
    glcompat::record_or_apply_texcoord({{s, t, 0.0f, 1.0f}});
}
void glTexCoord2fv(const GLfloat* v) {
    glcompat::record_or_apply_texcoord({{v[0], v[1], 0.0f, 1.0f}});
}
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
    glcompat::record_or_apply_texcoord({{s, t, r, 1.0f}});
}

// =========================================================================
// Display lists — record commands, replay on call.
// =========================================================================

GLuint glGenLists(GLsizei n) {
    auto& s = state();
    GLuint base = (GLuint)s.dlists.size();
    for (GLsizei i = 0; i < n; ++i) s.dlists.emplace_back();
    return base;
}
void glNewList(GLuint list, GLenum mode) {
    auto& s = state();
    while (s.dlists.size() <= list) s.dlists.emplace_back();
    s.dlists[list].clear();
    s.in_dlist = true;
    s.dlist_writing = list;
    s.dlist_mode = mode;
}
void glEndList(void) { state().in_dlist = false; }
void glCallList(GLuint list) {
    auto& s = state();
    if (list >= s.dlists.size()) return;
    // Execute recorded commands by translating each back into setter calls.
    // Saved as a contiguous vector — cannot be modified during iteration
    // (glCallList → glCallList nesting is rare in the corpus, ignore).
    const auto cmds = s.dlists[list];   // copy
    for (const auto& c : cmds) {
        switch (c.kind) {
            case glcompat::DListCmd::BEGIN:
                glcompat::record_or_apply_begin(c.mode); break;
            case glcompat::DListCmd::END:
                glcompat::record_or_apply_end(); break;
            case glcompat::DListCmd::VERTEX:
                glcompat::record_or_apply_vertex(c.v); break;
            case glcompat::DListCmd::COLOR:
                glcompat::record_or_apply_color(c.v); break;
            case glcompat::DListCmd::NORMAL:
                glcompat::record_or_apply_normal(c.n); break;
            case glcompat::DListCmd::TEXCOORD:
                glcompat::record_or_apply_texcoord(c.v); break;
            case glcompat::DListCmd::NORMALIZE: break;
        }
    }
}
void glDeleteLists(GLuint list, GLsizei range) {
    auto& s = state();
    for (GLsizei i = 0; i < range; ++i)
        if (list + i < s.dlists.size()) s.dlists[list + i].clear();
}
GLboolean glIsList(GLuint list) {
    return list < state().dlists.size() && !state().dlists[list].empty();
}

// =========================================================================
// Texture stubs — Sprint 38 will plug in glTexImage2D.
// =========================================================================

void glGenTextures(GLsizei n, GLuint* out) {
    auto& s = state();
    for (GLsizei i = 0; i < n; ++i) {
        out[i] = (GLuint)s.textures.size();
        s.textures.emplace_back();
    }
}
void glBindTexture(GLenum, GLuint id) { state().bound_tex = id; }
void glTexImage2D(GLenum, GLint, GLint internalformat,
                  GLsizei width, GLsizei height, GLint,
                  GLenum format, GLenum type, const GLvoid* pixels) {
    auto& s = state();
    if (s.bound_tex == 0 || s.bound_tex >= s.textures.size()) return;
    auto& tex = s.textures[s.bound_tex];
    tex.width = width; tex.height = height;
    tex.texels.assign((size_t)width * height, 0u);
    if (!pixels) return;
    const auto* src = (const GLubyte*)pixels;
    auto pack = [&](GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
               ((uint32_t)g <<  8) |  (uint32_t)r;
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            GLubyte r = 0, g = 0, b = 0, a = 255;
            if (type == GL_UNSIGNED_BYTE) {
                if (format == GL_RGB)         { r = src[0]; g = src[1]; b = src[2]; src += 3; }
                else if (format == GL_RGBA)   { r = src[0]; g = src[1]; b = src[2]; a = src[3]; src += 4; }
                else if (format == GL_LUMINANCE){ r = g = b = src[0]; src++; }
                else if (format == GL_LUMINANCE_ALPHA){ r = g = b = src[0]; a = src[1]; src += 2; }
                else if (format == GL_ABGR_EXT){ a = src[0]; b = src[1]; g = src[2]; r = src[3]; src += 4; }
                else { src++; }     // unsupported format — read 1 byte
            }
            tex.texels[(size_t)y * width + x] = pack(r, g, b, a);
        }
    }
    (void)internalformat;
}
void glCopyTexImage2D(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) {}
void glTexParameteri(GLenum, GLenum pname, GLint param) {
    auto& s = state();
    if (s.bound_tex == 0 || s.bound_tex >= s.textures.size()) return;
    auto& tex = s.textures[s.bound_tex];
    if (pname == GL_TEXTURE_MAG_FILTER || pname == GL_TEXTURE_MIN_FILTER)
        tex.linear = (param == GL_LINEAR);
    else if (pname == GL_TEXTURE_WRAP_S)
        tex.wrap_repeat_s = (param == GL_REPEAT);
    else if (pname == GL_TEXTURE_WRAP_T)
        tex.wrap_repeat_t = (param == GL_REPEAT);
}
void glTexParameterf(GLenum t, GLenum p, GLfloat v) { glTexParameteri(t, p, (GLint)v); }

}  // extern "C"
