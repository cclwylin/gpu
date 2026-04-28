// Sprint 37 — glmark2 follow-up #7 first slice.
//
// Drives the full ES 2.0 path with **real GLSL source** (pulsar.vert +
// light-basic.frag from `tests/glmark2/data/shaders/`), bypassing the
// hand-baked program catalogue. The new fallback inside
// `glcompat::es2::glLinkProgram` calls `gpu::glsl::compile` to lower
// the source pair to ISA on the fly.
//
// Asserts the 3-vertex (red / green / blue) test triangle paints to
// the framebuffer the same way the baked-program triangle does.

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

// Verbatim from tests/glmark2/data/shaders/pulsar.vert.
const char* kPulsarVS =
    "attribute vec3 position;\n"
    "attribute vec4 vtxcolor;\n"
    "attribute vec2 texcoord;\n"
    "attribute vec3 normal;\n"
    "\n"
    "uniform mat4 ModelViewProjectionMatrix;\n"
    "\n"
    "varying vec4 Color;\n"
    "varying vec2 TextureCoord;\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    Color = vtxcolor;\n"
    "    TextureCoord = texcoord;\n"
    "    gl_Position = ModelViewProjectionMatrix * vec4(position, 1.0);\n"
    "}\n";

// Verbatim from tests/glmark2/data/shaders/light-basic.frag.
const char* kBasicFS =
    "varying vec4 Color;\n"
    "varying vec2 TextureCoord;\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "    gl_FragColor = Color;\n"
    "}\n";

int main() {
    using gpu::glmark2_runner::HeadlessCanvas;

    constexpr int W = 64, H = 64;
    HeadlessCanvas canvas(W, H, 0.0f, 0.0f, 0.0f, 1.0f);

    // Compile + link.
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &kPulsarVS, nullptr);
    glCompileShader(vs);
    GLint vs_ok = 0; glGetShaderiv(vs, GL_COMPILE_STATUS, &vs_ok);
    if (!vs_ok) { std::fprintf(stderr, "FAIL: VS compile\n"); return 1; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &kBasicFS, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0; glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { std::fprintf(stderr, "FAIL: glLinkProgram (GLSL fallback)\n"); return 1; }

    // Look up locations exactly the way glmark2 does.
    GLint mvp_loc      = glGetUniformLocation(prog, "ModelViewProjectionMatrix");
    GLint pos_loc      = glGetAttribLocation (prog, "position");
    GLint color_loc    = glGetAttribLocation (prog, "vtxcolor");
    if (mvp_loc < 0 || pos_loc < 0 || color_loc < 0) {
        std::fprintf(stderr, "FAIL: locs mvp=%d pos=%d color=%d\n",
                     mvp_loc, pos_loc, color_loc);
        return 1;
    }

    // Identity MVP: clip space straight to NDC.
    const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    glUseProgram(prog);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, identity);

    // Three-vertex triangle. position is vec3 (the VS packs it into vec4
    // via vec4(position, 1.0)); vtxcolor is vec4.
    const float pos[] = {
         0.0f,  0.7f,  0.0f,
        -0.7f, -0.7f,  0.0f,
         0.7f, -0.7f,  0.0f,
    };
    const float col[] = {
        1, 0, 0, 1,
        0, 1, 0, 1,
        0, 0, 1, 1,
    };

    GLuint vbo[2];
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(col), col, GL_STATIC_DRAW);
    glEnableVertexAttribArray(color_loc);
    glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    canvas.swap();

    auto px = canvas.read_back();

    int painted = 0;
    int xmin = W, xmax = -1, ymin = H, ymax = -1;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (px[y * W + x] != 0xFF000000u) {
                ++painted;
                if (x < xmin) { xmin = x; }
                if (x > xmax) { xmax = x; }
                if (y < ymin) { ymin = y; }
                if (y > ymax) { ymax = y; }
            }
        }
    }
    if (painted < 100) {
        std::fprintf(stderr, "FAIL: painted=%d (expected >=100)\n", painted);
        return 1;
    }
    auto px_at = [&](int x, int y) -> uint32_t {
        if (x < 0) { x = 0; } else if (x >= W) { x = W - 1; }
        if (y < 0) { y = 0; } else if (y >= H) { y = H - 1; }
        return px[y * W + x];
    };
    const uint32_t apex   = px_at(W / 2,    ymax - 2);
    const uint32_t bleft  = px_at(xmin + 2, ymin + 2);
    const uint32_t bright = px_at(xmax - 2, ymin + 2);

    auto dom_red   = [](uint32_t p){ return r_of(p) > 100 && g_of(p) < 80 && b_of(p) < 80; };
    auto dom_green = [](uint32_t p){ return g_of(p) > 100 && r_of(p) < 80 && b_of(p) < 80; };
    auto dom_blue  = [](uint32_t p){ return b_of(p) > 100 && r_of(p) < 80 && g_of(p) < 80; };

    if (!dom_red(apex))    { std::fprintf(stderr, "FAIL: apex 0x%08x\n", apex);   return 1; }
    if (!dom_green(bleft)) { std::fprintf(stderr, "FAIL: bleft 0x%08x\n", bleft); return 1; }
    if (!dom_blue(bright)) { std::fprintf(stderr, "FAIL: bright 0x%08x\n", bright); return 1; }

    canvas.save_to_out_dir("glsl");
    std::printf("PASS — pulsar.vert + light-basic.frag compiled via GLSL "
                "frontend, painted %d pixels (apex=0x%08x bl=0x%08x br=0x%08x)\n",
                painted, apex, bleft, bright);
    return 0;
}
