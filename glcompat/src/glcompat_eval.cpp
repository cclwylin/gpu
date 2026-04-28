// Sprint 43 — fixed-function evaluators (glMap*, glEvalMesh*).
//
// Implements the GL eval-pipeline used by tests/examples/{evaltest,
// molehill,surfgrid}.c and a number of others. Real GL provides
// Bezier 1D & 2D evaluators with multiple "targets" (vertex / colour
// / texcoord / normal). We implement the vertex target in 3D and 4D;
// the colour / texture / normal targets are stored but only the
// VERTEX_3 / VERTEX_4 maps drive triangle emission.
//
// Coverage:
//   glMap1d  / glMap1f         (k-order Bezier curve, vertex 3/4)
//   glMap2d  / glMap2f         (k-order × l-order Bezier patch, vertex 3/4)
//   glMapGrid1d/2d/2f
//   glEvalCoord1d/2d
//   glEvalMesh1/2  (LINE + FILL; POINT skipped — no point primitive)

#include "glcompat_runtime.h"

#include <GL/gl.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct Map1 {
    GLenum   target = 0;
    int      stride = 0;
    int      order  = 0;
    double   u0 = 0, u1 = 1;
    std::vector<double> ctrl;
};
struct Map2 {
    GLenum   target = 0;
    int      ustride = 0, uorder = 0;
    int      vstride = 0, vorder = 0;
    double   u0 = 0, u1 = 1, v0 = 0, v1 = 1;
    std::vector<double> ctrl;
};

struct EvalState {
    Map1 map1[4];      // indexed by target offset (0=VERTEX_3, …)
    Map2 map2[4];
    int  u_segments = 1; double u_grid_lo = 0, u_grid_hi = 1;
    int  v_segments = 1; double v_grid_lo = 0, v_grid_hi = 1;
};
EvalState& es() { static EvalState s; return s; }

int target_idx(GLenum t) {
    switch (t) {
        case GL_MAP2_VERTEX_3:        case GL_MAP1_VERTEX_3:         return 0;
        case GL_MAP2_VERTEX_4:        case GL_MAP1_VERTEX_4:         return 1;
        case GL_MAP2_COLOR_4:         case GL_MAP1_COLOR_4:          return 2;
        case GL_MAP2_TEXTURE_COORD_2: case GL_MAP1_TEXTURE_COORD_2:  return 3;
        default:                                                     return 0;
    }
}
int target_components(GLenum t) {
    switch (t) {
        case GL_MAP1_VERTEX_3:
        case GL_MAP2_VERTEX_3:        return 3;
        case GL_MAP1_VERTEX_4:
        case GL_MAP2_VERTEX_4:        return 4;
        case GL_MAP1_COLOR_4:
        case GL_MAP2_COLOR_4:         return 4;
        case GL_MAP1_TEXTURE_COORD_2:
        case GL_MAP2_TEXTURE_COORD_2: return 2;
        default:                      return 4;
    }
}

// Bernstein polynomial B(n, i, t).
double bernstein(int n, int i, double t) {
    // Iterative computation for numerical stability and to avoid
    // factorials. C(n, i) * t^i * (1-t)^(n-i).
    double c = 1.0;
    for (int k = 0; k < i; ++k) c = c * (n - k) / (k + 1);
    return c * std::pow(t, i) * std::pow(1.0 - t, n - i);
}

void eval2_at(const Map2& m, double u, double v, double out[4]) {
    if (m.uorder == 0 || m.vorder == 0) return;
    const int comps = target_components(m.target);
    for (int c = 0; c < comps; ++c) out[c] = 0.0;
    out[3] = 1.0;
    const double tu = (m.u0 == m.u1) ? 0.0 : (u - m.u0) / (m.u1 - m.u0);
    const double tv = (m.v0 == m.v1) ? 0.0 : (v - m.v0) / (m.v1 - m.v0);
    const int n = m.uorder - 1, M = m.vorder - 1;
    for (int i = 0; i <= n; ++i) {
        const double bi = bernstein(n, i, tu);
        for (int j = 0; j <= M; ++j) {
            const double bj = bernstein(M, j, tv);
            const double w = bi * bj;
            const double* p = &m.ctrl[(size_t)i * m.ustride
                                     + (size_t)j * m.vstride];
            for (int c = 0; c < comps; ++c) out[c] += w * p[c];
        }
    }
}
void eval1_at(const Map1& m, double u, double out[4]) {
    const int comps = target_components(m.target);
    for (int c = 0; c < comps; ++c) out[c] = 0.0;
    out[3] = 1.0;
    const double t = (m.u0 == m.u1) ? 0.0 : (u - m.u0) / (m.u1 - m.u0);
    const int n = m.order - 1;
    for (int i = 0; i <= n; ++i) {
        const double b = bernstein(n, i, t);
        const double* p = &m.ctrl[(size_t)i * m.stride];
        for (int c = 0; c < comps; ++c) out[c] += b * p[c];
    }
}

// Per-eval point: emit a vertex via glcompat's immediate-mode path.
void emit_eval_point2(double u, double v) {
    auto& s = es();
    // Find the bound vertex map (3 or 4 component).
    Map2* m = nullptr;
    int comps = 3;
    if (s.map2[1].uorder > 0) { m = &s.map2[1]; comps = 4; }
    else if (s.map2[0].uorder > 0) { m = &s.map2[0]; comps = 3; }
    if (!m) return;
    double v4[4] = {0, 0, 0, 1};
    eval2_at(*m, u, v, v4);
    if (comps == 3) glVertex3f((float)v4[0], (float)v4[1], (float)v4[2]);
    else            glVertex4f((float)v4[0], (float)v4[1], (float)v4[2], (float)v4[3]);
}
void emit_eval_point1(double u) {
    auto& s = es();
    Map1* m = nullptr;
    int comps = 3;
    if (s.map1[1].order > 0) { m = &s.map1[1]; comps = 4; }
    else if (s.map1[0].order > 0) { m = &s.map1[0]; comps = 3; }
    if (!m) return;
    double v4[4] = {0, 0, 0, 1};
    eval1_at(*m, u, v4);
    if (comps == 3) glVertex3f((float)v4[0], (float)v4[1], (float)v4[2]);
    else            glVertex4f((float)v4[0], (float)v4[1], (float)v4[2], (float)v4[3]);
}

}  // namespace

extern "C" {

void glMap1d(GLenum target, GLdouble u0, GLdouble u1,
             GLint stride, GLint order, const GLdouble* points) {
    auto& m = es().map1[target_idx(target)];
    m.target = target; m.stride = stride; m.order = order;
    m.u0 = u0; m.u1 = u1;
    const int comps = target_components(target);
    m.ctrl.assign((size_t)stride * order, 0.0);
    for (int i = 0; i < order; ++i)
        for (int c = 0; c < comps; ++c)
            m.ctrl[(size_t)i * stride + c] = points[(size_t)i * stride + c];
}
void glMap1f(GLenum t, GLfloat u0, GLfloat u1,
             GLint stride, GLint order, const GLfloat* points) {
    std::vector<GLdouble> dp((size_t)stride * order);
    for (size_t i = 0; i < dp.size(); ++i) dp[i] = points[i];
    glMap1d(t, u0, u1, stride, order, dp.data());
}

void glMap2d(GLenum target,
             GLdouble u0, GLdouble u1, GLint ustride, GLint uorder,
             GLdouble v0, GLdouble v1, GLint vstride, GLint vorder,
             const GLdouble* points) {
    auto& m = es().map2[target_idx(target)];
    m.target = target;
    m.ustride = ustride; m.uorder = uorder;
    m.vstride = vstride; m.vorder = vorder;
    m.u0 = u0; m.u1 = u1; m.v0 = v0; m.v1 = v1;
    const size_t total = (size_t)ustride * uorder + (size_t)vstride * vorder;
    m.ctrl.assign(total + 16, 0.0);
    const int comps = target_components(target);
    for (int i = 0; i < uorder; ++i) {
        for (int j = 0; j < vorder; ++j) {
            const GLdouble* src = &points[(size_t)i * ustride
                                          + (size_t)j * vstride];
            GLdouble* dst       = &m.ctrl[(size_t)i * ustride
                                          + (size_t)j * vstride];
            for (int c = 0; c < comps; ++c) dst[c] = src[c];
        }
    }
}
void glMap2f(GLenum target,
             GLfloat u0, GLfloat u1, GLint ustride, GLint uorder,
             GLfloat v0, GLfloat v1, GLint vstride, GLint vorder,
             const GLfloat* points) {
    const size_t total = (size_t)ustride * uorder + (size_t)vstride * vorder + 16;
    std::vector<GLdouble> dp(total);
    const int comps = target_components(target);
    for (int i = 0; i < uorder; ++i)
        for (int j = 0; j < vorder; ++j)
            for (int c = 0; c < comps; ++c)
                dp[(size_t)i * ustride + (size_t)j * vstride + c]
                    = points[(size_t)i * ustride + (size_t)j * vstride + c];
    glMap2d(target, u0, u1, ustride, uorder, v0, v1, vstride, vorder, dp.data());
}

void glMapGrid1d(GLint un, GLdouble u0, GLdouble u1) {
    auto& s = es(); s.u_segments = un; s.u_grid_lo = u0; s.u_grid_hi = u1;
}
void glMapGrid2d(GLint un, GLdouble u0, GLdouble u1,
                 GLint vn, GLdouble v0, GLdouble v1) {
    auto& s = es();
    s.u_segments = un; s.u_grid_lo = u0; s.u_grid_hi = u1;
    s.v_segments = vn; s.v_grid_lo = v0; s.v_grid_hi = v1;
}
void glMapGrid2f(GLint un, GLfloat u0, GLfloat u1,
                 GLint vn, GLfloat v0, GLfloat v1) {
    glMapGrid2d(un, u0, u1, vn, v0, v1);
}

void glEvalCoord1d(GLdouble u)            { emit_eval_point1(u); }
void glEvalCoord2d(GLdouble u, GLdouble v){ emit_eval_point2(u, v); }

void glEvalMesh1(GLenum mode, GLint i1, GLint i2) {
    auto& s = es();
    if (i1 > i2) std::swap(i1, i2);
    if (mode == GL_LINE) {
        glBegin(GL_LINE_STRIP);
        for (int i = i1; i <= i2; ++i) {
            const double u = s.u_grid_lo
                + (s.u_grid_hi - s.u_grid_lo) * i / std::max(1, s.u_segments);
            emit_eval_point1(u);
        }
        glEnd();
    } else if (mode == GL_POINT) {
        // No point primitive — render as a degenerate line per pair.
        for (int i = i1; i + 1 <= i2; ++i) {
            glBegin(GL_LINE_STRIP);
            const double u  = s.u_grid_lo + (s.u_grid_hi - s.u_grid_lo) * i      / std::max(1, s.u_segments);
            const double u2 = s.u_grid_lo + (s.u_grid_hi - s.u_grid_lo) * (i+1)  / std::max(1, s.u_segments);
            emit_eval_point1(u); emit_eval_point1(u2); glEnd();
        }
    }
}

void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {
    auto& s = es();
    auto u_at = [&](int i) {
        return s.u_grid_lo + (s.u_grid_hi - s.u_grid_lo) * i
                              / std::max(1, s.u_segments);
    };
    auto v_at = [&](int j) {
        return s.v_grid_lo + (s.v_grid_hi - s.v_grid_lo) * j
                              / std::max(1, s.v_segments);
    };
    if (i1 > i2) std::swap(i1, i2);
    if (j1 > j2) std::swap(j1, j2);
    if (mode == GL_FILL) {
        for (int i = i1; i + 1 <= i2; ++i) {
            glBegin(GL_QUAD_STRIP);
            for (int j = j1; j <= j2; ++j) {
                emit_eval_point2(u_at(i),     v_at(j));
                emit_eval_point2(u_at(i + 1), v_at(j));
            }
            glEnd();
        }
    } else if (mode == GL_LINE) {
        for (int i = i1; i <= i2; ++i) {
            glBegin(GL_LINE_STRIP);
            for (int j = j1; j <= j2; ++j) emit_eval_point2(u_at(i), v_at(j));
            glEnd();
        }
        for (int j = j1; j <= j2; ++j) {
            glBegin(GL_LINE_STRIP);
            for (int i = i1; i <= i2; ++i) emit_eval_point2(u_at(i), v_at(j));
            glEnd();
        }
    }
}

void glEvalPoint1(GLint i) {
    auto& s = es();
    emit_eval_point1(s.u_grid_lo +
                     (s.u_grid_hi - s.u_grid_lo) * i / std::max(1, s.u_segments));
}
void glEvalPoint2(GLint i, GLint j) {
    auto& s = es();
    emit_eval_point2(s.u_grid_lo +
                     (s.u_grid_hi - s.u_grid_lo) * i / std::max(1, s.u_segments),
                     s.v_grid_lo +
                     (s.v_grid_hi - s.v_grid_lo) * j / std::max(1, s.v_segments));
}

}  // extern "C"
