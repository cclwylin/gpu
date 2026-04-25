// glut.h — minimal subset for tests/examples/*.c.
//
// Stubs everything that doesn't matter for offscreen rendering; runs
// the user's display callback once and exits.
#pragma once
#include <GL/gl.h>
#include <GL/glu.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLUT_RGB                 0x0000
#define GLUT_RGBA                0x0000
#define GLUT_INDEX               0x0001
#define GLUT_SINGLE              0x0000
#define GLUT_DOUBLE              0x0002
#define GLUT_DEPTH               0x0010
#define GLUT_STENCIL             0x0020
#define GLUT_ACCUM               0x0004
#define GLUT_ALPHA               0x0008
#define GLUT_MULTISAMPLE         0x0080
#define GLUT_BITMAP_9_BY_15      ((void*)2)
#define GLUT_BITMAP_8_BY_13      ((void*)3)
#define GLUT_BITMAP_TIMES_ROMAN_10 ((void*)4)
#define GLUT_BITMAP_TIMES_ROMAN_24 ((void*)5)
#define GLUT_BITMAP_HELVETICA_10 ((void*)6)
#define GLUT_BITMAP_HELVETICA_12 ((void*)7)
#define GLUT_BITMAP_HELVETICA_18 ((void*)8)
#define GLUT_STROKE_ROMAN        ((void*)0)
#define GLUT_STROKE_MONO_ROMAN   ((void*)1)
#define GLUT_NUM_DIALS           4
#define GLUT_NUM_BUTTONS         3
#define GLUT_NUM_TABLET_BUTTONS  4
#define GLUT_NUM_SPACEBALL_BUTTONS 8
#define GLUT_HAS_KEYBOARD        600
#define GLUT_HAS_MOUSE           601
#define GLUT_HAS_DIAL_AND_BUTTON_BOX 605
#define GLUT_ELAPSED_TIME        700
#define GLUT_WINDOW_BUFFER_SIZE  100
#define GLUT_WINDOW_DEPTH_SIZE   106
#define GLUT_WINDOW_STENCIL_SIZE 107
#define GLUT_WINDOW_RGBA         116
#define GLUT_WINDOW_DOUBLEBUFFER 115
#define GLUT_WINDOW_X            108
#define GLUT_WINDOW_Y            109
#define GLUT_SCREEN_WIDTH        200
#define GLUT_SCREEN_HEIGHT       201
#define GLUT_OVERLAY_DAMAGED     119
#define GLUT_OVERLAY_POSSIBLE    127
#define GLUT_GAME_MODE_LEFT      0
#define GLUT_TRANSPARENT_INDEX   118
#define GLUT_NORMAL_DAMAGED      120
#define GLUT_LEFT_OBSERVED_2     1
#define GLUT_STEREO              0x0100
#define GLUT_LUMINANCE           0x0200
#define GLUT_WINDOW_COLORMAP_SIZE 110
#define GLUT_NUM_BUTTON_BOX_BUTTONS 603
#define GLUT_NUM_TABLET_BUTTONS_2 604
#define GLUT_DEVICE_KEY_REPEAT   612
#define GLUT_JOYSTICK_BUTTONS    617
#define GLUT_OWNS_JOYSTICK       613
#define GLUT_DEVICE_IGNORE_KEY_REPEAT 611
#define GLUT_RENDERING_CONTEXT   125
#define GLUT_INIT_DISPLAY_MODE   124
#define GLUT_LEFT_BUTTON  0
#define GLUT_RIGHT_BUTTON 2
#define GLUT_MIDDLE_BUTTON 1
#define GLUT_LEFT  0
#define GLUT_RIGHT 2
#define GLUT_MIDDLE 1
#define GLUT_ENTERED 1
#define GLUT_LEFT_OBSERVED 0
#define GLUT_DOWN  0
#define GLUT_UP    1
#define GLUT_KEY_F1   1
#define GLUT_KEY_F2   2
#define GLUT_KEY_F3   3
#define GLUT_KEY_F4   4
#define GLUT_KEY_F5   5
#define GLUT_KEY_F6   6
#define GLUT_KEY_F7   7
#define GLUT_KEY_F8   8
#define GLUT_KEY_F9   9
#define GLUT_KEY_F10  10
#define GLUT_KEY_F11  11
#define GLUT_KEY_F12  12
#define GLUT_KEY_LEFT  100
#define GLUT_KEY_UP    101
#define GLUT_KEY_RIGHT 102
#define GLUT_KEY_DOWN  103
#define GLUT_KEY_PAGE_UP   104
#define GLUT_KEY_PAGE_DOWN 105
#define GLUT_KEY_HOME      106
#define GLUT_KEY_END       107
#define GLUT_KEY_INSERT    108
#define GLUT_VISIBLE      1
#define GLUT_NOT_VISIBLE  0
#define GLUT_ACTIVE_SHIFT 1
#define GLUT_ACTIVE_CTRL  2
#define GLUT_ACTIVE_ALT   4
#define GLUT_GAME_MODE_ACTIVE 0
#define GLUT_OVERLAY 1
#define GLUT_NORMAL  0
#define GLUT_CURSOR_INHERIT 0
#define GLUT_CURSOR_NONE    100
#define GLUT_CURSOR_CROSSHAIR 102
#define GLUT_DEVICE_PREVIOUS 0
#define GLUT_WINDOW_WIDTH 102
#define GLUT_WINDOW_HEIGHT 103

void glutInit(int* argc, char** argv);
void glutInitDisplayMode(unsigned int mode);
void glutInitDisplayString(const char* s);
void glutInitWindowSize(int w, int h);
void glutInitWindowPosition(int x, int y);
int  glutCreateWindow(const char* title);
int  glutCreateSubWindow(int win, int x, int y, int w, int h);
void glutDestroyWindow(int win);
void glutSetWindow(int win);
int  glutGetWindow(void);
void glutSetWindowTitle(const char* t);

void glutDisplayFunc(void (*f)(void));
void glutReshapeFunc(void (*f)(int, int));
void glutKeyboardFunc(void (*f)(unsigned char, int, int));
void glutSpecialFunc(void (*f)(int, int, int));
void glutMouseFunc(void (*f)(int, int, int, int));
void glutMotionFunc(void (*f)(int, int));
void glutPassiveMotionFunc(void (*f)(int, int));
void glutEntryFunc(void (*f)(int));
void glutVisibilityFunc(void (*f)(int));
void glutIdleFunc(void (*f)(void));
void glutTimerFunc(unsigned int ms, void (*f)(int), int v);
void glutMenuStateFunc(void (*f)(int));
void glutMenuStatusFunc(void (*f)(int, int, int));

int  glutCreateMenu(void (*f)(int));
void glutAddMenuEntry(const char* label, int v);
void glutAddSubMenu(const char* label, int sub);
void glutAttachMenu(int button);
void glutChangeToMenuEntry(int e, const char* label, int v);
void glutSetMenu(int m);
int  glutGetMenu(void);

void glutMainLoop(void);
void glutPostRedisplay(void);
void glutSwapBuffers(void);
void glutFullScreen(void);
void glutReshapeWindow(int w, int h);
void glutPositionWindow(int x, int y);
int  glutGet(GLenum key);
int  glutDeviceGet(GLenum key);
int  glutGetModifiers(void);
int  glutLayerGet(GLenum k);

void glutBitmapCharacter(void* font, int c);
int  glutStrokeCharacter(void* font, int c);
int  glutStrokeWidth(void* font, int c);

void glutSolidSphere(GLdouble r, GLint slices, GLint stacks);
void glutWireSphere(GLdouble r, GLint slices, GLint stacks);
void glutSolidCube(GLdouble s);
void glutWireCube(GLdouble s);
void glutSolidCone(GLdouble base, GLdouble h, GLint slices, GLint stacks);
void glutSolidTorus(GLdouble inner, GLdouble outer, GLint sides, GLint rings);
void glutSolidTeapot(GLdouble s);
void glutWireTeapot(GLdouble s);
void glutSolidIcosahedron(void);
void glutSolidDodecahedron(void);
void glutSolidTetrahedron(void);

void glutSetCursor(int);
void glutEstablishOverlay(void);
void glutHideOverlay(void);
void glutShowOverlay(void);
void glutPostOverlayRedisplay(void);
void glutOverlayDisplayFunc(void (*f)(void));
void glutHideWindow(void);
void glutShowWindow(void);
int  glutExtensionSupported(const char* name);
void glutSetColor(int idx, GLfloat r, GLfloat g, GLfloat b);
void glutDialsFunc(void (*f)(int, int));
void glutButtonBoxFunc(void (*f)(int, int));
void glutSpaceballMotionFunc(void (*f)(int, int, int));
void glutSpaceballRotateFunc(void (*f)(int, int, int));
void glutSpaceballButtonFunc(void (*f)(int, int));
void glutTabletButtonFunc(void (*f)(int, int, int, int));
void glutTabletMotionFunc(void (*f)(int, int));
void glutScaleBiasMenu(int, int, int, int);
int  glutUseLayer(GLenum);

#ifdef __cplusplus
}
#endif
