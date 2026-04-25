// Sprint 36 — minimal OpenGL 1.x compatibility header.
//
// This is *not* the system /usr/include/GL/gl.h. It declares only the
// subset of GL 1.x used by tests/examples/*.c and routes implementations
// to the gpu::pipeline (sw_ref / SC chain).
//
// Anything not declared here either: (a) was never used by the example
// corpus, or (b) will be added when the example that needs it gets
// covered.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef unsigned int  GLenum;
typedef unsigned char GLboolean;
typedef unsigned int  GLbitfield;
typedef signed char   GLbyte;
typedef short         GLshort;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int  GLuint;
typedef float         GLfloat;
typedef float         GLclampf;
typedef double        GLdouble;
typedef double        GLclampd;
typedef void          GLvoid;

#define GL_FALSE 0
#define GL_TRUE  1

// Version-gating macros that example code keys off via #ifdef.
#define GL_VERSION_1_1 1
#define GL_VERSION_1_2 1

// Buffer bits.
#define GL_COLOR_BUFFER_BIT      0x00004000
#define GL_DEPTH_BUFFER_BIT      0x00000100
#define GL_STENCIL_BUFFER_BIT    0x00000400
#define GL_ACCUM_BUFFER_BIT      0x00000200

// Enables.
#define GL_DEPTH_TEST            0x0B71
#define GL_BLEND                 0x0BE2
#define GL_LIGHTING              0x0B50
#define GL_LIGHT0                0x4000
#define GL_LIGHT1                0x4001
#define GL_LIGHT2                0x4002
#define GL_LIGHT3                0x4003
#define GL_LIGHT4                0x4004
#define GL_LIGHT5                0x4005
#define GL_LIGHT6                0x4006
#define GL_LIGHT7                0x4007
#define GL_NORMALIZE             0x0BA1
#define GL_CULL_FACE             0x0B44
#define GL_TEXTURE_2D            0x0DE1
#define GL_ALPHA_TEST            0x0BC0
#define GL_SCISSOR_TEST          0x0C11
#define GL_STENCIL_TEST          0x0B90
#define GL_COLOR_MATERIAL        0x0B57
#define GL_FOG                   0x0B60
#define GL_LINE_SMOOTH           0x0B20
#define GL_POINT_SMOOTH          0x0B10
#define GL_POLYGON_SMOOTH        0x0B41
#define GL_DITHER                0x0BD0
#define GL_AUTO_NORMAL           0x0D80

// Primitive types.
#define GL_POINTS                0x0000
#define GL_LINES                 0x0001
#define GL_LINE_LOOP             0x0002
#define GL_LINE_STRIP            0x0003
#define GL_TRIANGLES             0x0004
#define GL_TRIANGLE_STRIP        0x0005
#define GL_TRIANGLE_FAN          0x0006
#define GL_QUADS                 0x0007
#define GL_QUAD_STRIP            0x0008
#define GL_POLYGON               0x0009

// Matrix modes.
#define GL_MODELVIEW             0x1700
#define GL_PROJECTION            0x1701
#define GL_TEXTURE               0x1702

// Faces.
#define GL_FRONT                 0x0404
#define GL_BACK                  0x0405
#define GL_FRONT_AND_BACK        0x0408
#define GL_CCW                   0x0901
#define GL_CW                    0x0900

// Compare functions.
#define GL_NEVER                 0x0200
#define GL_LESS                  0x0201
#define GL_EQUAL                 0x0202
#define GL_LEQUAL                0x0203
#define GL_GREATER               0x0204
#define GL_NOTEQUAL              0x0205
#define GL_GEQUAL                0x0206
#define GL_ALWAYS                0x0207

// Blend factors.
#define GL_ZERO                  0
#define GL_ONE                   1
#define GL_SRC_COLOR             0x0300
#define GL_ONE_MINUS_SRC_COLOR   0x0301
#define GL_SRC_ALPHA             0x0302
#define GL_ONE_MINUS_SRC_ALPHA   0x0303
#define GL_DST_ALPHA             0x0304
#define GL_ONE_MINUS_DST_ALPHA   0x0305
#define GL_DST_COLOR             0x0306
#define GL_ONE_MINUS_DST_COLOR   0x0307

// Light parameters.
#define GL_AMBIENT               0x1200
#define GL_DIFFUSE               0x1201
#define GL_SPECULAR              0x1202
#define GL_POSITION              0x1203
#define GL_SPOT_DIRECTION        0x1204
#define GL_SPOT_EXPONENT         0x1205
#define GL_SPOT_CUTOFF           0x1206
#define GL_CONSTANT_ATTENUATION  0x1207
#define GL_LINEAR_ATTENUATION    0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#define GL_EMISSION              0x1600
#define GL_SHININESS             0x1601
#define GL_AMBIENT_AND_DIFFUSE   0x1602
#define GL_LIGHT_MODEL_AMBIENT          0x0B53
#define GL_LIGHT_MODEL_TWO_SIDE         0x0B52
#define GL_LIGHT_MODEL_LOCAL_VIEWER     0x0B51

// Texture.
#define GL_TEXTURE_MAG_FILTER    0x2800
#define GL_TEXTURE_MIN_FILTER    0x2801
#define GL_TEXTURE_WRAP_S        0x2802
#define GL_TEXTURE_WRAP_T        0x2803
#define GL_NEAREST               0x2600
#define GL_LINEAR                0x2601
#define GL_REPEAT                0x2901
#define GL_CLAMP                 0x2900
#define GL_CLAMP_TO_EDGE         0x812F
#define GL_RGB                   0x1907
#define GL_RGBA                  0x1908
#define GL_LUMINANCE             0x1909
#define GL_LUMINANCE_ALPHA       0x190A
#define GL_ABGR_EXT              0x8000
#define GL_UNSIGNED_BYTE         0x1401
#define GL_FLOAT                 0x1406

// Hints.
#define GL_FOG_HINT              0x0C54
#define GL_LINE_SMOOTH_HINT      0x0C52
#define GL_POINT_SMOOTH_HINT     0x0C51
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_DONT_CARE             0x1100
#define GL_FASTEST               0x1101
#define GL_NICEST                0x1102

// Shade model.
#define GL_FLAT                  0x1D00
#define GL_SMOOTH                0x1D01

// Get / query.
#define GL_VIEWPORT              0x0BA2
#define GL_MODELVIEW_MATRIX      0x0BA6
#define GL_PROJECTION_MATRIX     0x0BA7
#define GL_NO_ERROR              0
#define GL_VENDOR                0x1F00
#define GL_RENDERER              0x1F01
#define GL_VERSION               0x1F02
#define GL_EXTENSIONS            0x1F03

// ------------------------------------------------------------ entry points
void glBegin(GLenum mode);
void glEnd(void);

void glVertex2i(GLint x, GLint y);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex2fv(const GLfloat* v);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3fv(const GLfloat* v);
void glVertex3d(GLdouble x, GLdouble y, GLdouble z);
void glVertex3dv(const GLdouble* v);
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);

void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor3fv(const GLfloat* v);
void glColor3ub(GLubyte r, GLubyte g, GLubyte b);
void glColor3ubv(const GLubyte* v);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor4fv(const GLfloat* v);

void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glNormal3fv(const GLfloat* v);

void glTexCoord1f(GLfloat s);
void glTexCoord2f(GLfloat s, GLfloat t);
void glTexCoord2fv(const GLfloat* v);
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r);

void glClear(GLbitfield mask);
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glClearDepth(GLclampd d);
void glClearStencil(GLint s);
void glClearIndex(GLfloat c);

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h);

void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat* m);
void glMultMatrixf(const GLfloat* m);
void glPushMatrix(void);
void glPopMatrix(void);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
             GLdouble n, GLdouble f);
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
               GLdouble n, GLdouble f);

void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);

void glDepthFunc(GLenum func);
void glDepthMask(GLboolean flag);
void glBlendFunc(GLenum sf, GLenum df);
void glAlphaFunc(GLenum func, GLclampf ref);
void glCullFace(GLenum mode);
void glFrontFace(GLenum mode);
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void glShadeModel(GLenum mode);

// Lighting / materials.
void glLightf(GLenum light, GLenum pname, GLfloat p);
void glLightfv(GLenum light, GLenum pname, const GLfloat* p);
void glLightModelf(GLenum pname, GLfloat p);
void glLightModelfv(GLenum pname, const GLfloat* p);
void glLightModeli(GLenum pname, GLint p);
void glMaterialf(GLenum face, GLenum pname, GLfloat p);
void glMaterialfv(GLenum face, GLenum pname, const GLfloat* p);
void glColorMaterial(GLenum face, GLenum mode);

// Texture.
void glGenTextures(GLsizei n, GLuint* textures);
void glBindTexture(GLenum target, GLuint id);
void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const GLvoid* pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* p);
void glTexParameteriv(GLenum target, GLenum pname, const GLint* p);
void glVertex4dv(const GLdouble* v);
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void glVertex2d(GLdouble x, GLdouble y);
void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat,
                      GLint x, GLint y, GLsizei w, GLsizei h, GLint border);
#define GL_TEXTURE_COMPONENTS    GL_TEXTURE_INTERNAL_FORMAT
void glPixelStorei(GLenum pname, GLint param);
void glTexEnvf(GLenum target, GLenum pname, GLfloat p);
void glTexEnvi(GLenum target, GLenum pname, GLint p);
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat* p);
void glPixelStore(GLenum pname, GLint p);
void glPixelTransferf(GLenum pname, GLfloat p);
void glDrawBuffer(GLenum mode);
void glReadBuffer(GLenum mode);
void glDrawPixels(GLsizei w, GLsizei h, GLenum f, GLenum t, const GLvoid* p);
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*);
void glRasterPos2i(GLint x, GLint y);
void glRasterPos2f(GLfloat x, GLfloat y);
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
void glBitmap(GLsizei w, GLsizei h, GLfloat xb, GLfloat yb,
              GLfloat xa, GLfloat ya, const GLubyte* bits);
void glIndexi(GLint c);
void glIndexf(GLfloat c);
void glClipPlane(GLenum p, const GLdouble* eq);
void glFogf(GLenum p, GLfloat v);
void glFogi(GLenum p, GLint v);
void glFogfv(GLenum p, const GLfloat* v);
void glInitNames(void);
void glLoadName(GLuint n);
void glPushName(GLuint n);
void glPopName(void);
GLint glRenderMode(GLenum m);
void glSelectBuffer(GLsizei sz, GLuint* buf);
void glFeedbackBuffer(GLsizei sz, GLenum t, GLfloat* buf);
void glPassThrough(GLfloat token);
void glAccum(GLenum op, GLfloat val);
void glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glLogicOp(GLenum op);
void glVertex3i(GLint x, GLint y, GLint z);
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glColor4ubv(const GLubyte* v);
void glGetTexLevelParameteriv(GLenum, GLint, GLenum, GLint*);

// Eval / map (NURBS-like) — stubs.
void glMap1d(GLenum, GLdouble, GLdouble, GLint, GLint, const GLdouble*);
void glMap1f(GLenum, GLfloat,  GLfloat,  GLint, GLint, const GLfloat*);
void glMap2d(GLenum, GLdouble, GLdouble, GLint, GLint,
             GLdouble, GLdouble, GLint, GLint, const GLdouble*);
void glMap2f(GLenum, GLfloat,  GLfloat,  GLint, GLint,
             GLfloat,  GLfloat,  GLint, GLint, const GLfloat*);
void glMapGrid1d(GLint, GLdouble, GLdouble);
void glMapGrid2d(GLint, GLdouble, GLdouble, GLint, GLdouble, GLdouble);
void glMapGrid2f(GLint, GLfloat,  GLfloat,  GLint, GLfloat,  GLfloat);
void glEvalCoord1d(GLdouble u);
void glEvalCoord2d(GLdouble u, GLdouble v);
void glEvalMesh1(GLenum, GLint, GLint);
void glEvalMesh2(GLenum, GLint, GLint, GLint, GLint);
void glEvalPoint1(GLint i);
void glEvalPoint2(GLint i, GLint j);
#define GL_MAP1_VERTEX_3       0x0D90
#define GL_MAP2_VERTEX_3       0x0DB7
#define GL_AUTO_NORMAL_CAP     GL_AUTO_NORMAL

// Misc / no-ops.
void glFlush(void);
void glFinish(void);
void glHint(GLenum target, GLenum mode);
GLenum glGetError(void);
void glGetIntegerv(GLenum pname, GLint* v);
void glGetFloatv(GLenum pname, GLfloat* v);
void glGetBooleanv(GLenum pname, GLboolean* v);
void glGetDoublev(GLenum pname, GLdouble* v);
const GLubyte* glGetString(GLenum name);

// Display lists — minimal recording (compile + execute on call).
GLuint glGenLists(GLsizei n);
void glNewList(GLuint list, GLenum mode);
void glEndList(void);
void glCallList(GLuint list);
void glDeleteLists(GLuint list, GLsizei range);
GLboolean glIsList(GLuint list);

#define GL_COMPILE                0x1300
#define GL_COMPILE_AND_EXECUTE    0x1301

// PointSize / LineWidth.
void glPointSize(GLfloat s);
void glLineWidth(GLfloat w);
void glPolygonMode(GLenum face, GLenum mode);
#define GL_POINT                 0x1B00
#define GL_LINE                  0x1B01
#define GL_FILL                  0x1B02

// Stencil / scissor placeholders.
void glStencilFunc(GLenum func, GLint ref, GLuint mask);
void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
void glStencilMask(GLuint mask);
#define GL_KEEP                  0x1E00
#define GL_REPLACE               0x1E01
#define GL_INCR                  0x1E02
#define GL_DECR                  0x1E03
#define GL_INVERT                0x150A

// Texture env.
#define GL_TEXTURE_ENV           0x2300
#define GL_TEXTURE_ENV_MODE      0x2200
#define GL_TEXTURE_ENV_COLOR     0x2201
#define GL_DECAL                 0x2101
#define GL_MODULATE              0x2100
#define GL_REPLACE_TEX           0x1E01
#define GL_ALPHA                 0x1906
#define GL_INTENSITY             0x8049
#define GL_RGB8                  0x8051
#define GL_RGBA8                 0x8058

// Pixel store.
#define GL_UNPACK_ALIGNMENT      0x0CF5
#define GL_UNPACK_ROW_LENGTH     0x0CF2
#define GL_PACK_ALIGNMENT        0x0D05

// Buffer mode.
#define GL_BACK_LEFT             0x0402
#define GL_BACK_RIGHT            0x0403
#define GL_FRONT_LEFT            0x0400
#define GL_FRONT_RIGHT           0x0401
#define GL_LEFT                  0x0406
#define GL_RIGHT                 0x0407
#define GL_AUX0                  0x0409
#define GL_NONE                  0

// Fog.
#define GL_FOG_MODE              0x0B65
#define GL_FOG_DENSITY           0x0B62
#define GL_FOG_START             0x0B63
#define GL_FOG_END               0x0B64
#define GL_FOG_INDEX             0x0B61
#define GL_FOG_COLOR             0x0B66
#define GL_LINEAR_FOG            0x2601
#define GL_EXP                   0x0800
#define GL_EXP2                  0x0801

// Clip planes.
#define GL_CLIP_PLANE0           0x3000
#define GL_CLIP_PLANE1           0x3001
#define GL_CLIP_PLANE2           0x3002
#define GL_CLIP_PLANE3           0x3003
#define GL_CLIP_PLANE4           0x3004
#define GL_CLIP_PLANE5           0x3005

// Polygon stipple / line stipple — placeholder.
#define GL_POLYGON_STIPPLE       0x0B42
#define GL_LINE_STIPPLE          0x0B24

// Render modes.
#define GL_RENDER                0x1C00
#define GL_FEEDBACK              0x1C01
#define GL_SELECT                0x1C02

// Feedback tokens.
#define GL_PASS_THROUGH_TOKEN    0x0700
#define GL_POINT_TOKEN           0x0701
#define GL_LINE_TOKEN            0x0702
#define GL_POLYGON_TOKEN         0x0703
#define GL_BITMAP_TOKEN          0x0704
#define GL_DRAW_PIXEL_TOKEN      0x0705
#define GL_COPY_PIXEL_TOKEN      0x0706
#define GL_LINE_RESET_TOKEN      0x0707
#define GL_2D                    0x0600
#define GL_3D                    0x0601
#define GL_3D_COLOR              0x0602

// Logic op.
#define GL_AND                   0x1501
#define GL_OR                    0x1507
#define GL_XOR                   0x1506
#define GL_COPY                  0x1503
#define GL_INVERT_LOGIC          0x150A
#define GL_NOOP                  0x1505

// Misc.
#define GL_LIST_BASE             0x0B32
#define GL_INDEX_LOGIC_OP        0x0BF1

// glPixelZoom.
void glPixelZoom(GLfloat x, GLfloat y);
void glPolygonStipple(const GLubyte* m);
void glLineStipple(GLint factor, GLushort pattern);
void glPushAttrib(GLbitfield mask);
void glPopAttrib(void);
void glPushClientAttrib(GLbitfield mask);
void glPopClientAttrib(void);
void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void glTranslated(GLdouble x, GLdouble y, GLdouble z);
void glScaled(GLdouble x, GLdouble y, GLdouble z);

#define GL_LINEAR_MIPMAP_LINEAR  0x2703
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_RED_SCALE             0x0D14
#define GL_GREEN_SCALE           0x0D18
#define GL_BLUE_SCALE            0x0D1A
#define GL_ALPHA_SCALE           0x0D1C
#define GL_MAP1_VERTEX_4         0x0D91
#define GL_MAP2_VERTEX_4         0x0DB8
#define GL_COLOR_INDEXES         0x1603
#define GL_COLOR_CLEAR_VALUE     0x0C22
#define GL_COLOR_INDEX           0x1900
#define GL_INDEX_OFFSET          0x0D13
#define GL_TRANSFORM_BIT         0x00001000
#define GL_LIGHTING_BIT          0x00000040
#define GL_DEPTH_BUFFER_BIT_NEW  GL_DEPTH_BUFFER_BIT
#define GL_ENABLE_BIT            0x00002000
#define GL_TEXTURE_BIT           0x00040000
#define GL_CURRENT_BIT           0x00000001
#define GL_POLYGON_BIT           0x00000008
#define GL_VIEWPORT_BIT          0x00000800
#define GL_HINT_BIT              0x00008000
#define GL_LINE_BIT              0x00000004
#define GL_POINT_BIT             0x00000002
#define GL_PIXEL_MODE_BIT        0x00000020
#define GL_LIST_BIT              0x00020000
#define GL_FOG_BIT               0x00000080
#define GL_ACCUM_BUFFER_BIT_NEW  GL_ACCUM_BUFFER_BIT
#define GL_COLOR_BUFFER_BIT_NEW  GL_COLOR_BUFFER_BIT
#define GL_STENCIL_BUFFER_BIT_NEW GL_STENCIL_BUFFER_BIT
#define GL_ALL_ATTRIB_BITS       0x000FFFFF
#define GL_CLIENT_PIXEL_STORE_BIT 0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT 0x00000002
#define GL_CLIENT_ALL_ATTRIB_BITS  0xFFFFFFFF

// Texture / state queries.
#define GL_TEXTURE_WIDTH         0x1000
#define GL_TEXTURE_HEIGHT        0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_TEXTURE_BORDER        0x1005
#define GL_TEXTURE_BORDER_COLOR  0x1004
#define GL_TEXTURE_RED_SIZE      0x805C
#define GL_TEXTURE_GREEN_SIZE    0x805D
#define GL_TEXTURE_BLUE_SIZE     0x805E
#define GL_TEXTURE_ALPHA_SIZE    0x805F
#define GL_TEXTURE_LUMINANCE_SIZE 0x8060
#define GL_TEXTURE_INTENSITY_SIZE 0x8061
#define GL_POINT_SIZE            0x0B11
#define GL_POINT_SIZE_RANGE      0x0B12
#define GL_POINT_SIZE_GRANULARITY 0x0B13
#define GL_CURRENT_RASTER_POSITION 0x0B07
#define GL_CURRENT_RASTER_POSITION_VALID 0x0B08
#define GL_CURRENT_RASTER_DISTANCE 0x0B09
#define GL_CURRENT_RASTER_COLOR  0x0B04
#define GL_CURRENT_RASTER_INDEX  0x0B05
#define GL_CURRENT_RASTER_TEXTURE_COORDS 0x0B06
#define GL_MAP1_TEXTURE_COORD_1  0x0D93
#define GL_MAP1_TEXTURE_COORD_2  0x0D94
#define GL_MAP1_TEXTURE_COORD_3  0x0D95
#define GL_MAP1_TEXTURE_COORD_4  0x0D96
#define GL_MAP2_TEXTURE_COORD_1  0x0DB3
#define GL_MAP2_TEXTURE_COORD_2  0x0DB4
#define GL_MAP2_TEXTURE_COORD_3  0x0DB5
#define GL_MAP2_TEXTURE_COORD_4  0x0DB6
#define GL_MAP1_NORMAL           0x0D92
#define GL_MAP2_NORMAL           0x0DB2
#define GL_MAP1_INDEX            0x0D91
#define GL_MAP2_INDEX            0x0DB1
#define GL_MAP1_VERTEX_3_2       GL_MAP1_VERTEX_3
#define GL_MAP2_VERTEX_3_2       GL_MAP2_VERTEX_3
#define GL_RED_BIAS              0x0D15
#define GL_GREEN_BIAS            0x0D19
#define GL_BLUE_BIAS             0x0D1B
#define GL_ALPHA_BIAS            0x0D1D
#define GL_LINE_WIDTH            0x0B21
#define GL_LINE_WIDTH_RANGE      0x0B22
#define GL_LINE_WIDTH_GRANULARITY 0x0B23
#define GL_MAP1_COLOR_4          0x0D90
#define GL_MAP2_COLOR_4          0x0DB0
#define GL_VERTEX_ARRAY          0x8074
#define GL_NORMAL_ARRAY          0x8075
#define GL_COLOR_ARRAY           0x8076
#define GL_INDEX_ARRAY           0x8077
#define GL_TEXTURE_COORD_ARRAY   0x8078
#define GL_EDGE_FLAG_ARRAY       0x8079

void glVertex4fv(const GLfloat* v);
void glPushClientAttrib(GLbitfield);
void glPopClientAttrib(void);

#ifdef __cplusplus
}  // extern "C"
#endif
