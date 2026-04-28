// Sprint 38 — glmark2 follow-up #8 (texture path).
//
// Real GLSL ES 2.0 shader using `uniform sampler2D` + `texture2D(s, uv)`.
// 2×2 RGBA8 texture (R / G / B / W); fullscreen triangle pair textured
// with uv ∈ [0,1]. Verifies each quadrant's dominant colour after
// readback through the multi-unit ES 2.0 texture path:
//
//   glActiveTexture(GL_TEXTURE0)
//   glBindTexture(GL_TEXTURE_2D, id)
//   glTexImage2D(...)
//   glTexParameteri(...)
//   glUniform1i(sampler_loc, 0)
//   glDrawArrays(GL_TRIANGLES, 0, 6)

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

const char* kVS =
    "attribute vec3 position;\n"
    "attribute vec2 uv;\n"
    "uniform mat4 mvp;\n"
    "varying vec2 v_uv;\n"
    "void main(void) {\n"
    "    v_uv = uv;\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}\n";

const char* kFS =
    "uniform sampler2D tex;\n"
    "varying vec2 v_uv;\n"
    "void main(void) {\n"
    "    gl_FragColor = texture2D(tex, v_uv);\n"
    "}\n";

int main() {
    using gpu::glmark2_runner::HeadlessCanvas;

    constexpr int W = 64, H = 64;
    HeadlessCanvas canvas(W, H, 0.0f, 0.0f, 0.0f, 1.0f);

    // 2×2 RGBA8 texture: (0,0)=red (1,0)=green (0,1)=blue (1,1)=white
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    const uint8_t pixels[] = {
        255,   0,   0, 255,    0, 255,   0, 255,
          0,   0, 255, 255,  255, 255, 255, 255,
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Compile + link.
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &kVS, nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &kFS, nullptr); glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    GLint linked = 0; glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { std::fprintf(stderr, "FAIL: link\n"); return 1; }

    GLint mvp_loc = glGetUniformLocation(prog, "mvp");
    GLint tex_loc = glGetUniformLocation(prog, "tex");
    GLint pos_loc = glGetAttribLocation (prog, "position");
    GLint uv_loc  = glGetAttribLocation (prog, "uv");
    if (mvp_loc < 0 || tex_loc < 0 || pos_loc < 0 || uv_loc < 0) {
        std::fprintf(stderr, "FAIL: locs mvp=%d tex=%d pos=%d uv=%d\n",
                     mvp_loc, tex_loc, pos_loc, uv_loc);
        return 1;
    }

    glUseProgram(prog);
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, identity);
    glUniform1i(tex_loc, 0);                     // sampler reads unit 0

    // Fullscreen quad as 2 triangles. NDC y up matches uv y up so the
    // bottom-left of the texture maps to the bottom-left of the
    // framebuffer (red), top-right maps to top-right (white), etc.
    const float pos[] = {
        // tri 0: BL, BR, TR
        -1, -1, 0,
         1, -1, 0,
         1,  1, 0,
        // tri 1: BL, TR, TL
        -1, -1, 0,
         1,  1, 0,
        -1,  1, 0,
    };
    const float uvs[] = {
        0, 0,
        1, 0,
        1, 1,
        0, 0,
        1, 1,
        0, 1,
    };
    GLuint vbo[2];
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
    glEnableVertexAttribArray(uv_loc);
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    canvas.swap();

    auto px = canvas.read_back();

    auto px_at = [&](int x, int y) -> uint32_t {
        if (x < 0) { x = 0; } else if (x >= W) { x = W - 1; }
        if (y < 0) { y = 0; } else if (y >= H) { y = H - 1; }
        return px[y * W + x];
    };
    // Sample one pixel deep inside each NEAREST quadrant.
    const uint32_t bl = px_at(W / 4,         H / 4);            // red
    const uint32_t br = px_at(3 * W / 4,     H / 4);            // green
    const uint32_t tl = px_at(W / 4,         3 * H / 4);        // blue
    const uint32_t tr = px_at(3 * W / 4,     3 * H / 4);        // white

    auto dom_red    = [](uint32_t p){ return r_of(p) > 200 && g_of(p) < 30  && b_of(p) < 30;  };
    auto dom_green  = [](uint32_t p){ return g_of(p) > 200 && r_of(p) < 30  && b_of(p) < 30;  };
    auto dom_blue   = [](uint32_t p){ return b_of(p) > 200 && r_of(p) < 30  && g_of(p) < 30;  };
    auto dom_white  = [](uint32_t p){ return r_of(p) > 200 && g_of(p) > 200 && b_of(p) > 200; };

    if (!dom_red  (bl)) { std::fprintf(stderr, "FAIL: bl 0x%08x (expect red)\n",   bl); return 1; }
    if (!dom_green(br)) { std::fprintf(stderr, "FAIL: br 0x%08x (expect green)\n", br); return 1; }
    if (!dom_blue (tl)) { std::fprintf(stderr, "FAIL: tl 0x%08x (expect blue)\n",  tl); return 1; }
    if (!dom_white(tr)) { std::fprintf(stderr, "FAIL: tr 0x%08x (expect white)\n", tr); return 1; }

    canvas.save_to_out_dir("textured_quad");
    std::printf("PASS — textured quad: bl=0x%08x br=0x%08x tl=0x%08x tr=0x%08x\n",
                bl, br, tl, tr);
    return 0;
}
