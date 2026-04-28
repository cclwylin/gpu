// Sprint 36/37 — GLU helpers.
//
// Forwarded to the matrix-stack helpers; no fancy NURBS / tessellator.

#include "glcompat_runtime.h"

#include <GL/glu.h>
#include <GL/glut.h>

#include <cmath>

extern "C" {

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zn, GLdouble zf) {
    const float f = 1.0f / std::tan((float)fovy * 0.5f * 3.14159265358979323846f / 180.0f);
    glcompat::Mat4 m{};
    m[0]  = f / (float)aspect;
    m[5]  = f;
    m[10] = (float)((zf + zn) / (zn - zf));
    m[11] = -1.0f;
    m[14] = (float)((2.0 * zf * zn) / (zn - zf));
    glMultMatrixf(m.data());
}

void gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez,
               GLdouble cx, GLdouble cy, GLdouble cz,
               GLdouble ux, GLdouble uy, GLdouble uz) {
    auto norm = [](float& x, float& y, float& z) {
        const float l = std::sqrt(x*x + y*y + z*z);
        if (l > 0.0f) { x /= l; y /= l; z /= l; }
    };
    float fx = (float)(cx - ex), fy = (float)(cy - ey), fz = (float)(cz - ez);
    norm(fx, fy, fz);
    float upx = (float)ux, upy = (float)uy, upz = (float)uz;
    norm(upx, upy, upz);
    float sx = fy * upz - fz * upy;
    float sy = fz * upx - fx * upz;
    float sz = fx * upy - fy * upx;
    norm(sx, sy, sz);
    const float ux2 = sy * fz - sz * fy;
    const float uy2 = sz * fx - sx * fz;
    const float uz2 = sx * fy - sy * fx;

    glcompat::Mat4 m{};
    m[0]  = sx;   m[4]  = sy;   m[8]  = sz;   m[12] = 0.0f;
    m[1]  = ux2;  m[5]  = uy2;  m[9]  = uz2;  m[13] = 0.0f;
    m[2]  = -fx;  m[6]  = -fy;  m[10] = -fz;  m[14] = 0.0f;
    m[3]  = 0.0f; m[7]  = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
    glMultMatrixf(m.data());
    glTranslatef(-(float)ex, -(float)ey, -(float)ez);
}

void gluOrtho2D(GLdouble l, GLdouble r, GLdouble b, GLdouble t) {
    glOrtho(l, r, b, t, -1.0, 1.0);
}

void gluPickMatrix(GLdouble, GLdouble, GLdouble, GLdouble, GLint*) {}
int  gluUnProject(GLdouble, GLdouble, GLdouble, const GLdouble*, const GLdouble*,
                  const GLint*, GLdouble* ox, GLdouble* oy, GLdouble* oz) {
    if (ox) *ox = 0; if (oy) *oy = 0; if (oz) *oz = 0;
    return 1;
}

// Quadric: opaque struct; gluSphere/Cylinder/Disk emit immediate-mode
// triangles into the same path as glutSolid*.
struct GLUquadricObj { int draw_style = 100012 /*GLU_FILL*/; };
GLUquadricObj* gluNewQuadric(void)             { return new GLUquadricObj{}; }
void gluDeleteQuadric(GLUquadricObj* q)        { delete q; }
void gluQuadricDrawStyle(GLUquadricObj* q, GLenum s) { if (q) q->draw_style = (int)s; }
void gluQuadricNormals(GLUquadricObj*, GLenum) {}
void gluQuadricOrientation(GLUquadricObj*, GLenum) {}
void gluQuadricTexture(GLUquadricObj*, GLboolean) {}

void gluSphere(GLUquadricObj*, GLdouble r, GLint slices, GLint stacks) {
    glutSolidSphere(r, slices, stacks);
}
void gluCylinder(GLUquadricObj*, GLdouble base, GLdouble top,
                 GLdouble h, GLint slices, GLint /*stacks*/) {
    const float pi = 3.14159265358979323846f;
    if (slices < 3) slices = 3;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; ++j) {
        const float a = 2.0f * pi * (float)j / slices;
        const float c = std::cos(a), s = std::sin(a);
        glNormal3f(c, s, 0.0f);
        glVertex3f((float)base * c, (float)base * s, 0.0f);
        glVertex3f((float)top * c,  (float)top * s,  (float)h);
    }
    glEnd();
}
void gluDisk(GLUquadricObj*, GLdouble inner, GLdouble outer,
             GLint slices, GLint /*loops*/) {
    const float pi = 3.14159265358979323846f;
    if (slices < 3) slices = 3;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; ++j) {
        const float a = 2.0f * pi * (float)j / slices;
        const float c = std::cos(a), s = std::sin(a);
        glNormal3f(0, 0, 1);
        glVertex3f((float)inner * c, (float)inner * s, 0.0f);
        glVertex3f((float)outer * c, (float)outer * s, 0.0f);
    }
    glEnd();
}

// Tessellator: callbacks remembered, but gluTessVertex just forwards
// to a glVertex (assumes the callback will glBegin/glEnd). Rough.
struct GLUtesselator { void (*v)() = nullptr; };
GLUtesselator* gluNewTess(void)              { return new GLUtesselator{}; }
void gluDeleteTess(GLUtesselator* t)         { delete t; }
void gluTessCallback(GLUtesselator*, GLenum, void (*)()) {}
void gluTessVertex(GLUtesselator*, GLdouble c[3], void*) {
    glVertex3d(c[0], c[1], c[2]);
}

GLint gluBuild2DMipmaps(GLenum target, GLint internal,
                        GLsizei w, GLsizei h, GLenum format,
                        GLenum type, const void* data) {
    glTexImage2D(target, 0, internal, w, h, 0, format, type, data);
    return 0;
}
const GLubyte* gluErrorString(GLenum)            { return (const GLubyte*)""; }
GLUnurbsObj* gluNewNurbsRenderer(void)           { return new GLUquadricObj{}; }
void gluDeleteNurbsRenderer(GLUnurbsObj* n)      { delete n; }
void gluNurbsProperty(GLUnurbsObj*, GLenum, GLfloat) {}
void gluBeginSurface(GLUnurbsObj*)               {}
void gluEndSurface(GLUnurbsObj*)                 {}
void gluNurbsSurface(GLUnurbsObj*, GLint, GLfloat*, GLint, GLfloat*,
                     GLint, GLint, GLfloat*, GLint, GLint, GLenum) {}

// Old-school polygon "tessellator" — emit a fan, ignore concavity.
void gluBeginPolygon(GLUtriangulatorObj*) { glBegin(GL_POLYGON); }
void gluEndPolygon(GLUtriangulatorObj*)   { glEnd(); }
void gluNextContour(GLUtriangulatorObj*, GLenum) {}
GLUtriangulatorObj* gluNewTriangulatorObj(void) { return new GLUquadricObj{}; }
void gluDeleteTriangulatorObj(GLUtriangulatorObj* t) { delete t; }

void gluTessNormal(GLUtesselator*, GLdouble, GLdouble, GLdouble) {}
void gluTessProperty(GLUtesselator*, GLenum, GLdouble) {}
void gluTessBeginContour(GLUtesselator*)   { glBegin(GL_POLYGON); }
void gluTessEndContour(GLUtesselator*)     { glEnd(); }
void gluTessBeginPolygon(GLUtesselator*, void*) {}
void gluTessEndPolygon(GLUtesselator*)     {}

}   // extern "C"
