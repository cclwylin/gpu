// Sprint 39 — first slice of "SC chain runs glmark2".
//
// Captures the same simple-triangle ES 2.0 draw as `glmark2.triangle`
// into a .scene file, then forks `sc_pattern_runner` to replay it
// through the cycle-accurate chain (CP / SC / SC→PA / PA / PA→RS / RS
// / RS→PFO / PFO / PFO→TBF / TBF / RSV). Reads the resulting PPM and
// asserts dominant red/green/blue at the expected sample points —
// i.e. the SC chain produced equivalent pixels to the sw_ref path.

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <GL/gl.h>

#include "baked_programs.h"
#include "canvas_headless.h"
#include "glcompat_runtime.h"

namespace {

inline uint8_t r_of(uint32_t p) { return  p        & 0xFF; }
inline uint8_t g_of(uint32_t p) { return (p >>  8) & 0xFF; }
inline uint8_t b_of(uint32_t p) { return (p >> 16) & 0xFF; }

// Read a P6 PPM into a packed-RGBA8 buffer.
std::vector<uint32_t> read_ppm_p6(const std::string& path,
                                  int* W_out, int* H_out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string magic; f >> magic;
    if (magic != "P6") return {};
    int W, H, maxv; f >> W >> H >> maxv;
    f.get();
    std::vector<uint8_t> raw((size_t)W * H * 3);
    f.read(reinterpret_cast<char*>(raw.data()), (std::streamsize)raw.size());
    if (!f) return {};
    std::vector<uint32_t> out((size_t)W * H);
    for (size_t i = 0; i < (size_t)W * H; ++i) {
        const uint8_t R = raw[3 * i + 0];
        const uint8_t G = raw[3 * i + 1];
        const uint8_t B = raw[3 * i + 2];
        out[i] = (uint32_t)R | ((uint32_t)G << 8) | ((uint32_t)B << 16) | (0xFFu << 24);
    }
    *W_out = W; *H_out = H;
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <sc_pattern_runner-path>\n", argv[0]);
        return 2;
    }
    const std::string sc_runner = argv[1];

    using gpu::glmark2_runner::HeadlessCanvas;
    using gpu::glmark2_runner::register_baked_programs;
    using gpu::glmark2_runner::kPosColorVs;
    using gpu::glmark2_runner::kPosColorFs;

    register_baked_programs();

    // Toggle ES 2.0 scene capture BEFORE first glClear/glDrawArrays so
    // the canvas's clear lands as a CLEAR op in the captured scene.
    glcompat::set_es2_scene_capture(true);

    constexpr int W = 64, H = 64;
    HeadlessCanvas canvas(W, H, 0.0f, 0.0f, 0.0f, 1.0f);

    // Same payload as scene_triangle.cpp.
    const float pos[] = {
         0.0f,   0.7f,  0.0f, 1.0f,
        -0.7f,  -0.7f,  0.0f, 1.0f,
         0.7f,  -0.7f,  0.0f, 1.0f,
    };
    const float col[] = {
        1, 0, 0, 1,
        0, 1, 0, 1,
        0, 0, 1, 1,
    };
    GLuint vbo[2]; glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(col), col, GL_STATIC_DRAW);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char* vs_src = kPosColorVs.c_str();
    glShaderSource(vs, 1, &vs_src, nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fs_src = kPosColorFs.c_str();
    glShaderSource(fs, 1, &fs_src, nullptr); glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    GLint linked = 0; glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { std::fprintf(stderr, "FAIL: link\n"); return 1; }

    glUseProgram(prog);
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(prog, "u_mvp"), 1, GL_FALSE, identity);
    GLint pos_loc = glGetAttribLocation(prog, "a_pos");
    GLint col_loc = glGetAttribLocation(prog, "a_color");
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 4, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(col_loc);
    glVertexAttribPointer(col_loc, 4, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    canvas.swap();

    // Drop the sw_ref readback into out/ for visual A/B against the SC
    // result that lands a few lines below.
    canvas.save_to_out_dir("to_sc", ".sw.ppm");

    // Dump the captured scene + invoke sc_pattern_runner on it. If
    // GLMARK2_OUT_DIR is set, persist both the scene and SC PPM there
    // alongside the sw_ref PPM; otherwise fall back to /tmp.
    std::string base = "/tmp/glmark2_to_sc";
    if (const char* env = std::getenv("GLMARK2_OUT_DIR")) {
        base = std::string(env) + "/glmark2_to_sc";
    }
    const std::string scene_path = base + ".scene";
    const std::string out_ppm    = base + ".sc.ppm";
    glcompat::save_scene_to(scene_path);

    {
        std::ifstream check(scene_path);
        if (!check) {
            std::fprintf(stderr, "FAIL: scene file not written at %s\n",
                         scene_path.c_str());
            return 1;
        }
    }

    const std::string cmd = sc_runner + " " + scene_path + " " + out_ppm;
    std::fprintf(stderr, "[sc] running: %s\n", cmd.c_str());
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::fprintf(stderr, "FAIL: sc_pattern_runner rc=%d\n", rc);
        return 1;
    }

    int sw = 0, sh = 0;
    auto px = read_ppm_p6(out_ppm, &sw, &sh);
    if (px.empty() || sw != W || sh != H) {
        std::fprintf(stderr, "FAIL: PPM read (%dx%d, %zu px)\n", sw, sh, px.size());
        return 1;
    }

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
        std::fprintf(stderr, "FAIL: SC produced only %d painted pixels\n", painted);
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
    if (!dom_red  (apex))   { std::fprintf(stderr, "FAIL: SC apex 0x%08x\n", apex);   return 1; }
    if (!dom_green(bleft))  { std::fprintf(stderr, "FAIL: SC bl   0x%08x\n", bleft);  return 1; }
    if (!dom_blue (bright)) { std::fprintf(stderr, "FAIL: SC br   0x%08x\n", bright); return 1; }

    std::printf("PASS — SC chain rendered glmark2 triangle: %d pixels "
                "apex=0x%08x bl=0x%08x br=0x%08x\n",
                painted, apex, bleft, bright);
    return 0;
}
