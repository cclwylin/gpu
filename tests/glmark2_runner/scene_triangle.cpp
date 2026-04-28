// Sprint 36 — glmark2 follow-up #6/#8 second slice.
//
// Drives the full ES 2.0 path: glGenBuffers / glBufferData /
// glCreateShader / glShaderSource / glCompileShader / glAttachShader /
// glLinkProgram / glUseProgram / glGetUniformLocation /
// glUniformMatrix4fv / glEnableVertexAttribArray /
// glVertexAttribPointer / glDrawArrays.
//
// Renders one triangle (red apex, green left-base, blue right-base)
// at NDC fullscreen with identity MVP, then asserts that the apex,
// bottom-left, and bottom-right pixels are predominantly the right
// colour.

#include <cstdio>
#include <cstdint>
#include <vector>

#include <GL/gl.h>

#include "baked_programs.h"
#include "canvas_headless.h"

namespace {
inline uint8_t r_of(uint32_t p) { return  p        & 0xFF; }
inline uint8_t g_of(uint32_t p) { return (p >>  8) & 0xFF; }
inline uint8_t b_of(uint32_t p) { return (p >> 16) & 0xFF; }
}

int main() {
    using gpu::glmark2_runner::HeadlessCanvas;
    using gpu::glmark2_runner::register_baked_programs;
    using gpu::glmark2_runner::kPosColorVs;
    using gpu::glmark2_runner::kPosColorFs;

    register_baked_programs();

    constexpr int W = 64, H = 64;
    HeadlessCanvas canvas(W, H, 0.0f, 0.0f, 0.0f, 1.0f);

    // Interleaved-free attribs: separate VBOs for pos and colour.
    const float pos[] = {
        // x      y      z     w
         0.0f,   0.7f,  0.0f, 1.0f,    // apex
        -0.7f,  -0.7f,  0.0f, 1.0f,    // bottom-left
         0.7f,  -0.7f,  0.0f, 1.0f,    // bottom-right
    };
    const float col[] = {
        1, 0, 0, 1,        // red
        0, 1, 0, 1,        // green
        0, 0, 1, 1,        // blue
    };

    GLuint vbo[2];
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(col), col, GL_STATIC_DRAW);

    // Compile / link.
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char* vs_src = kPosColorVs.c_str();
    glShaderSource(vs, 1, &vs_src, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fs_src = kPosColorFs.c_str();
    glShaderSource(fs, 1, &fs_src, nullptr);
    glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { std::fprintf(stderr, "FAIL: program not linked\n"); return 1; }

    // Identity MVP — clip space already maps into NDC.
    const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    glUseProgram(prog);
    GLint mvp_loc = glGetUniformLocation(prog, "u_mvp");
    if (mvp_loc < 0) { std::fprintf(stderr, "FAIL: u_mvp loc < 0\n"); return 1; }
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, identity);

    GLint pos_loc = glGetAttribLocation(prog, "a_pos");
    GLint col_loc = glGetAttribLocation(prog, "a_color");
    if (pos_loc < 0 || col_loc < 0) {
        std::fprintf(stderr, "FAIL: attrib locs %d/%d\n", pos_loc, col_loc);
        return 1;
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 4, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(col_loc);
    glVertexAttribPointer(col_loc, 4, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    canvas.swap();

    auto px = canvas.read_back();

    int painted = 0;
    for (auto v : px) if (v != 0xFF000000u) ++painted;
    if (painted < 100) {
        std::fprintf(stderr, "FAIL: only %d painted pixels\n", painted);
        return 1;
    }

    // Sample three points biased into each vertex's region. Apex sits
    // around (W/2, H*0.85), bottom-left around (W*0.18, H*0.18),
    // bottom-right around (W*0.82, H*0.18). Coords are in fb space:
    // glcompat maps NDC.y *up* (matches GL convention), so apex y is
    // near the top of the buffer.
    auto px_at = [&](int x, int y) -> uint32_t {
        if (x < 0) { x = 0; } else if (x >= W) { x = W - 1; }
        if (y < 0) { y = 0; } else if (y >= H) { y = H - 1; }
        return px[y * W + x];
    };
    // Find the painted bbox so the corner samples adapt to whatever
    // sub-pixel rounding the rasterizer settles on.
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
    const uint32_t apex   = px_at(W / 2,    ymax - 2);
    const uint32_t bleft  = px_at(xmin + 2, ymin + 2);
    const uint32_t bright = px_at(xmax - 2, ymin + 2);

    auto dominant_red   = [](uint32_t p){ return r_of(p) > 100 && g_of(p) < 80  && b_of(p) < 80;  };
    auto dominant_green = [](uint32_t p){ return g_of(p) > 100 && r_of(p) < 80  && b_of(p) < 80;  };
    auto dominant_blue  = [](uint32_t p){ return b_of(p) > 100 && r_of(p) < 80  && g_of(p) < 80;  };

    if (!dominant_red(apex)) {
        std::fprintf(stderr, "FAIL: apex pixel not red (0x%08x)\n", apex); return 1;
    }
    if (!dominant_green(bleft)) {
        std::fprintf(stderr, "FAIL: bottom-left pixel not green (0x%08x)\n", bleft); return 1;
    }
    if (!dominant_blue(bright)) {
        std::fprintf(stderr, "FAIL: bottom-right pixel not blue (0x%08x)\n", bright); return 1;
    }

    canvas.save_to_out_dir("triangle");
    std::printf("PASS — triangle painted %d pixels (apex=0x%08x bl=0x%08x br=0x%08x)\n",
                painted, apex, bleft, bright);
    return 0;
}
