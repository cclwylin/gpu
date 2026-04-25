// Sprint 36 — GLUT stubs.
//
// Headless replacement: no actual window, no event loop. glutMainLoop
// runs the registered display callback exactly once after invoking
// reshape with the requested window size, then writes the framebuffer
// to a PPM and exits cleanly.

#include "glcompat_runtime.h"

#include <GL/glut.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
struct GlutState {
    int  win_w = 256, win_h = 256;
    int  win_x = 0, win_y = 0;
    void (*display_cb)(void) = nullptr;
    void (*reshape_cb)(int, int) = nullptr;
    void (*keyboard_cb)(unsigned char, int, int) = nullptr;
    void (*idle_cb)(void) = nullptr;
};
GlutState& gs() { static GlutState g; return g; }
}  // namespace

extern "C" {

void glutInit(int*, char**)                         {}
void glutInitDisplayMode(unsigned int)              {}
void glutInitDisplayString(const char*)             {}
void glutInitWindowSize(int w, int h)               { gs().win_w = w; gs().win_h = h; }
void glutInitWindowPosition(int x, int y)           { gs().win_x = x; gs().win_y = y; }
int  glutCreateWindow(const char*)                  { return 1; }
int  glutCreateSubWindow(int, int, int, int, int)   { return 1; }
void glutDestroyWindow(int)                         {}
void glutSetWindow(int)                             {}
int  glutGetWindow(void)                            { return 1; }
void glutSetWindowTitle(const char*)                {}

void glutDisplayFunc(void (*f)(void))                       { gs().display_cb = f; }
void glutReshapeFunc(void (*f)(int, int))                   { gs().reshape_cb = f; }
void glutKeyboardFunc(void (*f)(unsigned char, int, int))   { gs().keyboard_cb = f; }
void glutSpecialFunc(void (*)(int, int, int))               {}
void glutMouseFunc(void (*)(int, int, int, int))            {}
void glutMotionFunc(void (*)(int, int))                     {}
void glutPassiveMotionFunc(void (*)(int, int))              {}
void glutEntryFunc(void (*)(int))                           {}
void glutVisibilityFunc(void (*)(int))                      {}
void glutIdleFunc(void (*f)(void))                          { gs().idle_cb = f; }
void glutTimerFunc(unsigned int, void (*)(int), int)        {}
void glutMenuStateFunc(void (*)(int))                       {}
void glutMenuStatusFunc(void (*)(int, int, int))            {}

int  glutCreateMenu(void (*)(int))                          { return 1; }
void glutAddMenuEntry(const char*, int)                     {}
void glutAddSubMenu(const char*, int)                       {}
void glutAttachMenu(int)                                    {}
void glutChangeToMenuEntry(int, const char*, int)           {}
void glutSetMenu(int)                                       {}
int  glutGetMenu(void)                                      { return 1; }

void glutPostRedisplay(void)                                {}
void glutSwapBuffers(void)                                  {}
void glutFullScreen(void)                                   {}
void glutReshapeWindow(int w, int h)                        { gs().win_w = w; gs().win_h = h; }
void glutPositionWindow(int, int)                           {}
int  glutGet(GLenum k) {
    switch (k) {
        case GLUT_WINDOW_WIDTH:        return gs().win_w;
        case GLUT_WINDOW_HEIGHT:       return gs().win_h;
        // Lie generously about buffer sizes so feature-detect early-
        // exits don't trip. We don't actually have these buffers, but
        // when the example uses them we silently no-op (or stencil
        // through sw_ref's actual stencil support).
        case GLUT_WINDOW_BUFFER_SIZE:  return 32;
        case GLUT_WINDOW_DEPTH_SIZE:   return 24;
        case GLUT_WINDOW_STENCIL_SIZE: return 8;
        case GLUT_WINDOW_COLORMAP_SIZE: return 256;
        case GLUT_WINDOW_NUM_SAMPLES:  return 4;
        case GLUT_SCREEN_WIDTH:        return 1920;
        case GLUT_SCREEN_HEIGHT:       return 1080;
        case GLUT_ELAPSED_TIME:        return 0;
        default:                       return 0;
    }
}
// Likewise: claim every device + every overlay capability is present;
// the example calls back into our stubs which silently no-op.
int  glutDeviceGet(GLenum)                                  { return 1; }
int  glutGetModifiers(void)                                 { return 0; }
int  glutLayerGet(GLenum)                                   { return 1; }

void glutBitmapCharacter(void*, int)                        {}
int  glutStrokeCharacter(void*, int)                        { return 0; }
int  glutStrokeWidth(void*, int)                            { return 0; }

void glutSetCursor(int)                                     {}
void glutEstablishOverlay(void)                             {}
void glutHideOverlay(void)                                  {}
void glutShowOverlay(void)                                  {}
void glutPostOverlayRedisplay(void)                         {}
void glutOverlayDisplayFunc(void (*)(void))                 {}
void glutHideWindow(void)                                   {}
void glutShowWindow(void)                                   {}
int  glutExtensionSupported(const char*)                    { return 1; }    // claim everything; we stub silently
void glutSetColor(int, GLfloat, GLfloat, GLfloat)           {}
void glutDialsFunc(void (*)(int, int))                      {}
void glutButtonBoxFunc(void (*)(int, int))                  {}
void glutSpaceballMotionFunc(void (*)(int, int, int))       {}
void glutSpaceballRotateFunc(void (*)(int, int, int))       {}
void glutSpaceballButtonFunc(void (*)(int, int))            {}
void glutTabletButtonFunc(void (*)(int, int, int, int))     {}
void glutTabletMotionFunc(void (*)(int, int))               {}
int  glutUseLayer(GLenum)                                   { return 0; }

void glutMainLoop(void) {
    auto& g = gs();
    if (g.reshape_cb) g.reshape_cb(g.win_w, g.win_h);
    if (g.display_cb) g.display_cb();
    glcompat::save_framebuffer();
    std::exit(0);
}

}  // extern "C"
