// Sprint 36 — GLUT stubs.
//
// Headless replacement: no actual window, no event loop. glutMainLoop
// runs the registered display callback exactly once after invoking
// reshape with the requested window size, then writes the framebuffer
// to a PPM and exits cleanly.

#include "glcompat_runtime.h"
#include "glcompat_font_bitmap.h"

#include <GL/glut.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

// Sprint 41 — minimal multi-window + overlay support.
//
//  * `windows[0]` = main window, created on first glutCreateWindow.
//  * Each glutCreateSubWindow appends a child to `windows[]` with
//    its own viewport rect. glutDisplayFunc registers on the
//    most-recently-created window (matches real GLUT semantics).
//  * Overlay = a sibling display callback that runs after the main
//    display in glutMainLoop (needed for oversphere.c, which does
//    all its drawing in the overlay path).
struct GlutWindow {
    int  x = 0, y = 0, w = 256, h = 256;
    void (*display_cb)(void) = nullptr;
    void (*reshape_cb)(int, int) = nullptr;
    void (*overlay_cb)(void) = nullptr;
};

struct GlutState {
    int  win_w = 256, win_h = 256;
    int  win_x = 0, win_y = 0;
    std::vector<GlutWindow> windows = {GlutWindow{}};
    int  current = 0;       // index into windows[]; updated by Set/Create
    void (*keyboard_cb)(unsigned char, int, int) = nullptr;
    void (*idle_cb)(void) = nullptr;
};
GlutState& gs() { static GlutState g; return g; }
GlutWindow& cw() { return gs().windows[gs().current]; }
}  // namespace

extern "C" {

void glutInit(int*, char**)                         {}
void glutInitDisplayMode(unsigned int)              {}
void glutInitDisplayString(const char*)             {}
void glutInitWindowSize(int w, int h)               { gs().win_w = w; gs().win_h = h; }
void glutInitWindowPosition(int x, int y)           { gs().win_x = x; gs().win_y = y; }
int  glutCreateWindow(const char*) {
    auto& g = gs();
    // The sole entry created at static-init covers the main window.
    g.current = 0;
    g.windows[0].x = g.win_x;
    g.windows[0].y = g.win_y;
    g.windows[0].w = g.win_w;
    g.windows[0].h = g.win_h;
    return 1;
}
int  glutCreateSubWindow(int /*parent*/, int x, int y, int w, int h) {
    auto& g = gs();
    g.windows.push_back({x, y, w, h, nullptr, nullptr, nullptr});
    g.current = (int)g.windows.size() - 1;
    return g.current + 1;     // GLUT IDs are 1-based
}
void glutDestroyWindow(int)                         {}
void glutSetWindow(int id) {
    auto& g = gs();
    const int i = id - 1;
    if (i >= 0 && i < (int)g.windows.size()) g.current = i;
}
int  glutGetWindow(void)                            { return gs().current + 1; }
void glutSetWindowTitle(const char*)                {}

void glutDisplayFunc(void (*f)(void))               { cw().display_cb = f; }
void glutReshapeFunc(void (*f)(int, int))           { cw().reshape_cb = f; }
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

void glutBitmapCharacter(void* /*font*/, int ch) {
    // We have one font (8×13). Other GLUT_BITMAP_* fonts route to the
    // same data — visual fidelity is best-effort.
    if (ch < 0x20 || ch > 0x7E) {
        // outside printable ASCII: just advance.
        glBitmap(0, 0, 0, 0, (GLfloat)glcompat::font::kBitmap8x13_Advance, 0, nullptr);
        return;
    }
    const uint8_t* glyph = glcompat::font::bitmap_8x13[ch - 0x20];
    // Convert TOP-down storage to BOTTOM-up rows expected by glBitmap.
    static thread_local uint8_t flipped[13];
    for (int r = 0; r < 13; ++r) flipped[r] = glyph[12 - r];
    glBitmap(8, 13, 0, 2,
             (GLfloat)glcompat::font::kBitmap8x13_Advance, 0, flipped);
}
// Stroke characters: rendered as a series of GL_LINE_STRIP segments.
// Real GLUT has a Hershey-style stroke font; we use a simple "filled
// rectangle outline" stand-in so the test produces visible content
// without a 3KB stroke table. Each glyph advances by 100 units
// (matches the real Helvetica metrics within ~5%).
int  glutStrokeCharacter(void* /*font*/, int ch) {
    if (ch < 0x20 || ch > 0x7E) return 100;
    // Outline of a small rectangle for any printable ASCII.
    glBegin(GL_LINE_LOOP);
    glVertex2f(10.0f,  0.0f);
    glVertex2f(90.0f,  0.0f);
    glVertex2f(90.0f, 80.0f);
    glVertex2f(10.0f, 80.0f);
    glEnd();
    glTranslatef(100.0f, 0.0f, 0.0f);
    return 100;
}
int  glutStrokeWidth(void*, int) { return 100; }

void glutSetCursor(int)                                     {}
void glutEstablishOverlay(void)                             {}
void glutHideOverlay(void)                                  {}
void glutShowOverlay(void)                                  {}
void glutPostOverlayRedisplay(void)                         {}
void glutOverlayDisplayFunc(void (*f)(void))                { cw().overlay_cb = f; }
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
    // Run reshape + display for every window we know about. Subwindows
    // inherit the main reshape if they didn't register their own. We
    // restore the GL viewport per-window so each draws into its own
    // rectangle of the shared framebuffer.
    for (size_t i = 0; i < g.windows.size(); ++i) {
        g.current = (int)i;
        auto& w = g.windows[i];
        if (i == 0) {
            // Main window: full requested size.
            glViewport(0, 0, g.win_w, g.win_h);
            if (w.reshape_cb) w.reshape_cb(g.win_w, g.win_h);
            else              glViewport(0, 0, g.win_w, g.win_h);
            if (w.display_cb) w.display_cb();
            // Run the overlay callback (if any) right after main —
            // captures cases like oversphere.c which puts all its
            // visible content in the overlay path.
            if (w.overlay_cb) w.overlay_cb();
        } else {
            // Subwindow: position + size known from glutCreateSubWindow.
            glViewport(w.x, w.y, w.w, w.h);
            if (w.reshape_cb) w.reshape_cb(w.w, w.h);
            if (w.display_cb) w.display_cb();
            if (w.overlay_cb) w.overlay_cb();
        }
    }
    glcompat::save_framebuffer();
    glcompat::save_scene();
    std::exit(0);
}

}  // extern "C"
