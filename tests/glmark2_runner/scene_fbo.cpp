// Sprint 38 — glmark2 follow-up #8 (FBO render-to-texture).
//
// Two-pass test:
//   pass 1: bind a 32×32 GL_TEXTURE_2D as GL_COLOR_ATTACHMENT0 of an
//           FBO; render an RGB-corner triangle into it.
//   pass 2: bind the default fb; render a fullscreen quad textured
//           with that same texture.
//
// Asserts the dominant-channel pixels appear in the corresponding
// quadrants of the default fb after pass 2.

#include <cstdio>
#include <cstdint>
#include <vector>

#include <GL/gl.h>

#include "canvas_headless.h"

namespace {
inline uint8_t r_of(uint32_t p) { return  p        & 0xFF; }
inline uint8_t g_of(uint32_t p) { return (p >>  8) & 0xFF; }
inline uint8_t b_of(uint32_t p) { return (p >> 16) & 0xFF; }
}  // namespace

// Pass 1 — colour-passthrough VS+FS (renders the RGB triangle).
const char* kColorVS =
    "attribute vec3 position;\n"
    "attribute vec4 vtxcolor;\n"
    "uniform mat4 mvp;\n"
    "varying vec4 v_color;\n"
    "void main(void) {\n"
    "    v_color = vtxcolor;\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}\n";
const char* kColorFS =
    "varying vec4 v_color;\n"
    "void main(void) { gl_FragColor = v_color; }\n";

// Pass 2 — sampler2D fullscreen quad, samples the FBO-attached texture.
const char* kTexVS =
    "attribute vec3 position;\n"
    "attribute vec2 uv;\n"
    "uniform mat4 mvp;\n"
    "varying vec2 v_uv;\n"
    "void main(void) {\n"
    "    v_uv = uv;\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}\n";
const char* kTexFS =
    "uniform sampler2D tex;\n"
    "varying vec2 v_uv;\n"
    "void main(void) { gl_FragColor = texture2D(tex, v_uv); }\n";

int main() {
    using gpu::glmark2_runner::HeadlessCanvas;

    constexpr int W = 64, H = 64;
    constexpr int FBO_W = 32, FBO_H = 32;

    HeadlessCanvas canvas(W, H, 0.0f, 0.0f, 0.0f, 1.0f);

    // ---- Allocate render target texture for pass 1 ----
    GLuint rt_tex = 0;
    glGenTextures(1, &rt_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rt_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FBO_W, FBO_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // ---- Allocate FBO + attach texture ----
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, rt_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "FAIL: fbo not complete\n"); return 1;
    }

    // ---- Pass 1: RGB triangle → FBO-attached texture ----
    glViewport(0, 0, FBO_W, FBO_H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint cv = glCreateShader(GL_VERTEX_SHADER);   glShaderSource(cv, 1, &kColorVS, nullptr); glCompileShader(cv);
    GLuint cf = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(cf, 1, &kColorFS, nullptr); glCompileShader(cf);
    GLuint cp = glCreateProgram();
    glAttachShader(cp, cv); glAttachShader(cp, cf); glLinkProgram(cp);
    GLint cp_ok = 0; glGetProgramiv(cp, GL_LINK_STATUS, &cp_ok);
    if (!cp_ok) { std::fprintf(stderr, "FAIL: pass1 link\n"); return 1; }

    GLint cp_mvp   = glGetUniformLocation(cp, "mvp");
    GLint cp_pos   = glGetAttribLocation (cp, "position");
    GLint cp_color = glGetAttribLocation (cp, "vtxcolor");
    glUseProgram(cp);
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(cp_mvp, 1, GL_FALSE, identity);

    const float tri_pos[] = {
         0.0f,  0.7f, 0,
        -0.7f, -0.7f, 0,
         0.7f, -0.7f, 0,
    };
    const float tri_col[] = {
        1, 0, 0, 1,
        0, 1, 0, 1,
        0, 0, 1, 1,
    };
    GLuint vbo[2]; glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri_pos), tri_pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(cp_pos);
    glVertexAttribPointer(cp_pos, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri_col), tri_col, GL_STATIC_DRAW);
    glEnableVertexAttribArray(cp_color);
    glVertexAttribPointer(cp_color, 4, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // ---- Pass 2: sample rt_tex onto a fullscreen quad of the default fb ----
    glBindFramebuffer(GL_FRAMEBUFFER, 0);    // copy_fb_to_attached_tex pushes pass-1 pixels into rt_tex
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint tv = glCreateShader(GL_VERTEX_SHADER);   glShaderSource(tv, 1, &kTexVS, nullptr); glCompileShader(tv);
    GLuint tf = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(tf, 1, &kTexFS, nullptr); glCompileShader(tf);
    GLuint tp = glCreateProgram();
    glAttachShader(tp, tv); glAttachShader(tp, tf); glLinkProgram(tp);
    GLint tp_ok = 0; glGetProgramiv(tp, GL_LINK_STATUS, &tp_ok);
    if (!tp_ok) { std::fprintf(stderr, "FAIL: pass2 link\n"); return 1; }

    GLint tp_mvp = glGetUniformLocation(tp, "mvp");
    GLint tp_tex = glGetUniformLocation(tp, "tex");
    GLint tp_pos = glGetAttribLocation (tp, "position");
    GLint tp_uv  = glGetAttribLocation (tp, "uv");
    glUseProgram(tp);
    glUniformMatrix4fv(tp_mvp, 1, GL_FALSE, identity);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rt_tex);
    glUniform1i(tp_tex, 0);

    const float quad_pos[] = {
        -1, -1, 0,
         1, -1, 0,
         1,  1, 0,
        -1, -1, 0,
         1,  1, 0,
        -1,  1, 0,
    };
    const float quad_uv[] = {
        0, 0, 1, 0, 1, 1,
        0, 0, 1, 1, 0, 1,
    };
    GLuint vbo2[2]; glGenBuffers(2, vbo2);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_pos), quad_pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(tp_pos);
    glVertexAttribPointer(tp_pos, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_uv), quad_uv, GL_STATIC_DRAW);
    glEnableVertexAttribArray(tp_uv);
    glVertexAttribPointer(tp_uv, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    canvas.swap();

    auto px = canvas.read_back();

    // The triangle in pass 1 (apex at NDC.y=+0.7) maps to upper-half of
    // rt_tex; pass 2 samples it onto the full default fb. So the apex
    // colour (red) should land near the top, bottom-left/right (green/blue)
    // near the bottom corners.
    auto px_at = [&](int x, int y) -> uint32_t {
        if (x < 0) { x = 0; } else if (x >= W) { x = W - 1; }
        if (y < 0) { y = 0; } else if (y >= H) { y = H - 1; }
        return px[y * W + x];
    };

    // Find painted bbox so the corner samples adapt to rasterizer rounding.
    int xmin = W, xmax = -1, ymin = H, ymax = -1;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (px[y * W + x] != 0xFF000000u) {
                if (x < xmin) { xmin = x; }
                if (x > xmax) { xmax = x; }
                if (y < ymin) { ymin = y; }
                if (y > ymax) { ymax = y; }
            }
        }
    }
    if (xmax < 0) { std::fprintf(stderr, "FAIL: nothing painted\n"); return 1; }
    const uint32_t apex   = px_at(W / 2,    ymax - 4);
    const uint32_t bleft  = px_at(xmin + 4, ymin + 4);
    const uint32_t bright = px_at(xmax - 4, ymin + 4);

    auto dom_red   = [](uint32_t p){ return r_of(p) > 100 && g_of(p) < 80 && b_of(p) < 80; };
    auto dom_green = [](uint32_t p){ return g_of(p) > 100 && r_of(p) < 80 && b_of(p) < 80; };
    auto dom_blue  = [](uint32_t p){ return b_of(p) > 100 && r_of(p) < 80 && g_of(p) < 80; };
    if (!dom_red  (apex))   { std::fprintf(stderr, "FAIL: apex   0x%08x\n", apex);   return 1; }
    if (!dom_green(bleft))  { std::fprintf(stderr, "FAIL: bleft  0x%08x\n", bleft);  return 1; }
    if (!dom_blue (bright)) { std::fprintf(stderr, "FAIL: bright 0x%08x\n", bright); return 1; }

    canvas.save_to_out_dir("fbo");
    std::printf("PASS — render-to-tex + sample: apex=0x%08x bl=0x%08x br=0x%08x\n",
                apex, bleft, bright);
    return 0;
}
