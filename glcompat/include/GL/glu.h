// glu.h — minimal subset.
#pragma once
#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zn, GLdouble zf);
void gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez,
               GLdouble cx, GLdouble cy, GLdouble cz,
               GLdouble ux, GLdouble uy, GLdouble uz);
void gluOrtho2D(GLdouble l, GLdouble r, GLdouble b, GLdouble t);
void gluPickMatrix(GLdouble x, GLdouble y, GLdouble dw, GLdouble dh,
                   GLint vp[4]);
int  gluUnProject(GLdouble wx, GLdouble wy, GLdouble wz,
                  const GLdouble m[16], const GLdouble p[16],
                  const GLint vp[4],
                  GLdouble* ox, GLdouble* oy, GLdouble* oz);

// Quadrics / NURBS / triangulator — opaque pointers (legacy alias names).
typedef struct GLUquadricObj GLUquadricObj;
typedef GLUquadricObj GLUquadric;
typedef struct GLUquadricObj GLUtriangulatorObj;
typedef struct GLUquadricObj GLUnurbsObj;
GLUquadricObj* gluNewQuadric(void);
void gluDeleteQuadric(GLUquadricObj* q);
void gluQuadricDrawStyle(GLUquadricObj* q, GLenum style);
void gluQuadricNormals(GLUquadricObj* q, GLenum normals);
void gluQuadricOrientation(GLUquadricObj* q, GLenum orient);
void gluQuadricTexture(GLUquadricObj* q, GLboolean t);
void gluSphere(GLUquadricObj* q, GLdouble radius, GLint slices, GLint stacks);
void gluCylinder(GLUquadricObj* q, GLdouble base, GLdouble top,
                 GLdouble height, GLint slices, GLint stacks);
void gluDisk(GLUquadricObj* q, GLdouble inner, GLdouble outer,
             GLint slices, GLint loops);

// Tessellation — stubs that just call the vertex callback for the
// CONTOUR points (no actual tessellation; works for convex polygons).
typedef struct GLUtesselator GLUtesselator;
GLUtesselator* gluNewTess(void);
void gluDeleteTess(GLUtesselator* t);
void gluTessCallback(GLUtesselator* t, GLenum which, void (*fn)());
void gluTessVertex(GLUtesselator* t, GLdouble coords[3], void* data);

#define GLU_FILL                100012
#define GLU_LINE                100011
#define GLU_POINT               100010
#define GLU_SILHOUETTE          100013
#define GLU_SMOOTH              100000
#define GLU_FLAT                100001
#define GLU_NONE                100002
#define GLU_OUTSIDE             100020
#define GLU_INSIDE              100021
#define GLU_TESS_BEGIN          100100
#define GLU_TESS_VERTEX         100101
#define GLU_TESS_END            100102
#define GLU_BEGIN               100100
#define GLU_VERTEX              100101
#define GLU_END                 100102
#define GLU_ERROR               100103
#define GLU_EDGE_FLAG           100104
#define GLU_OUT_OF_MEMORY       100902

GLint gluBuild2DMipmaps(GLenum target, GLint internal,
                        GLsizei w, GLsizei h, GLenum format,
                        GLenum type, const void* data);
const GLubyte* gluErrorString(GLenum err);
GLUnurbsObj* gluNewNurbsRenderer(void);
void gluDeleteNurbsRenderer(GLUnurbsObj*);
void gluNurbsProperty(GLUnurbsObj*, GLenum, GLfloat);
void gluBeginSurface(GLUnurbsObj*);
void gluEndSurface(GLUnurbsObj*);
void gluNurbsSurface(GLUnurbsObj*, GLint, GLfloat*, GLint, GLfloat*,
                     GLint, GLint, GLfloat*, GLint, GLint, GLenum);

// gluBeginPolygon-style "old-school" tessellator (just emits geometry
// directly via glBegin/glEnd as a fan).
void gluBeginPolygon(GLUtriangulatorObj*);
void gluEndPolygon(GLUtriangulatorObj*);
void gluNextContour(GLUtriangulatorObj*, GLenum);
GLUtriangulatorObj* gluNewTriangulatorObj(void);
void gluDeleteTriangulatorObj(GLUtriangulatorObj*);
void gluTessNormal(GLUtesselator*, GLdouble x, GLdouble y, GLdouble z);
void gluTessProperty(GLUtesselator*, GLenum, GLdouble);
void gluTessBeginContour(GLUtesselator*);
void gluTessEndContour(GLUtesselator*);
void gluTessBeginPolygon(GLUtesselator*, void*);
void gluTessEndPolygon(GLUtesselator*);
#define GLU_INTERIOR        100120
#define GLU_EXTERIOR        100121
#define GLU_UNKNOWN         100124
#define GLU_TESS_WINDING_RULE 100140
#define GLU_TESS_WINDING_ODD  100130
#define GLU_TESS_BEGIN_DATA   100106
#define GLU_TESS_VERTEX_DATA  100107
#define GLU_TESS_END_DATA     100108
#define GLU_TESS_COMBINE      100105
#define GLU_TESS_COMBINE_DATA 100111
#define GLU_TESS_ERROR_DATA   100109
#define GLU_TESS_EDGE_FLAG_DATA 100110
#define GLU_SAMPLING_TOLERANCE 100203
#define GLU_DISPLAY_MODE       100204
#define GLU_OUTLINE_POLYGON    100240
#define GLU_OUTLINE_PATCH      100241
#define GLU_FILL_POLYGON       100012

#ifdef __cplusplus
}
#endif
