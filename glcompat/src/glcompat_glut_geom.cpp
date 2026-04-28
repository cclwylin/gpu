// Sprint 37 — glut solid geometry helpers.
//
// Each function emits the geometry through glBegin/glEnd, so they
// flow through our normal immediate-mode pipeline. Wireframe vs
// solid distinction is collapsed to "solid" — line rasterisation is
// out of scope for the example corpus.

#include <GL/gl.h>
#include <GL/glut.h>

#include <cmath>
#include <initializer_list>

extern "C" {

void glutSolidCube(GLdouble s) {
    const float h = (float)(s * 0.5);
    static const float n[6][3] = {
        { 0,  0,  1}, { 0,  0, -1}, { 1,  0,  0},
        {-1,  0,  0}, { 0,  1,  0}, { 0, -1,  0},
    };
    static const float v[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},   // +Z
        {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}},   // -Z
        {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}},   // +X
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}},   // -X
        {{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1}},   // +Y
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}},   // -Y
    };
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; ++f) {
        glNormal3f(n[f][0], n[f][1], n[f][2]);
        for (int k = 0; k < 4; ++k)
            glVertex3f(v[f][k][0] * h, v[f][k][1] * h, v[f][k][2] * h);
    }
    glEnd();
}
void glutWireCube(GLdouble s) { glutSolidCube(s); }

void glutSolidSphere(GLdouble r, GLint slices, GLint stacks) {
    const float pi = 3.14159265358979323846f;
    if (slices < 3) slices = 3;
    if (stacks < 2) stacks = 2;
    for (int i = 0; i < stacks; ++i) {
        const float lat0 = pi * (-0.5f + (float)i / stacks);
        const float lat1 = pi * (-0.5f + (float)(i + 1) / stacks);
        const float z0 = std::sin(lat0), zr0 = std::cos(lat0);
        const float z1 = std::sin(lat1), zr1 = std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            const float lng = 2.0f * pi * (float)j / slices;
            const float x = std::cos(lng), y = std::sin(lng);
            glNormal3f(x * zr0, y * zr0, z0);
            glVertex3f(x * zr0 * (float)r, y * zr0 * (float)r, z0 * (float)r);
            glNormal3f(x * zr1, y * zr1, z1);
            glVertex3f(x * zr1 * (float)r, y * zr1 * (float)r, z1 * (float)r);
        }
        glEnd();
    }
}
void glutWireSphere(GLdouble r, GLint sl, GLint st) { glutSolidSphere(r, sl, st); }

void glutSolidCone(GLdouble base, GLdouble h, GLint slices, GLint /*stacks*/) {
    const float pi = 3.14159265358979323846f;
    if (slices < 3) slices = 3;
    glBegin(GL_TRIANGLES);
    for (int j = 0; j < slices; ++j) {
        const float t0 = 2.0f * pi * (float)j / slices;
        const float t1 = 2.0f * pi * (float)(j + 1) / slices;
        glNormal3f(std::cos((t0 + t1) * 0.5f), std::sin((t0 + t1) * 0.5f), 0.5f);
        glVertex3f(0, 0, (float)h);
        glVertex3f((float)base * std::cos(t0), (float)base * std::sin(t0), 0);
        glVertex3f((float)base * std::cos(t1), (float)base * std::sin(t1), 0);
    }
    glEnd();
}

void glutSolidTorus(GLdouble inner, GLdouble outer, GLint sides, GLint rings) {
    const float pi = 3.14159265358979323846f;
    if (sides < 3) sides = 3;
    if (rings < 3) rings = 3;
    for (int i = 0; i < rings; ++i) {
        const float u0 = 2 * pi * (float)i / rings;
        const float u1 = 2 * pi * (float)(i + 1) / rings;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= sides; ++j) {
            const float v = 2 * pi * (float)j / sides;
            const float cv = std::cos(v), sv = std::sin(v);
            for (float u : {u0, u1}) {
                const float cu = std::cos(u), su = std::sin(u);
                const float r = (float)outer + (float)inner * cv;
                const float x = r * cu, y = r * su, z = (float)inner * sv;
                glNormal3f(cv * cu, cv * su, sv);
                glVertex3f(x, y, z);
            }
        }
        glEnd();
    }
}

// Out-of-scope solids — emit a tiny pyramid as a placeholder so
// programs at least don't crash silently.
static void placeholder_solid() {
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 0, 1); glVertex3f(0, 0.5f, 0); glVertex3f(-0.5f, -0.5f, 0); glVertex3f(0.5f, -0.5f, 0);
    glEnd();
}
void glutSolidTeapot(GLdouble)        { placeholder_solid(); }
void glutWireTeapot(GLdouble)         { placeholder_solid(); }
void glutSolidIcosahedron(void)       { placeholder_solid(); }
void glutSolidDodecahedron(void)      { placeholder_solid(); }
void glutSolidTetrahedron(void)       { placeholder_solid(); }

}  // extern "C"
