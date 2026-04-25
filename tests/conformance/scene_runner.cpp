// Sprint 11 conformance harness.
//
// Reads a tiny line-oriented .scene file, runs it through sw_ref using a
// pass-through VS+FS, then asserts simple expectations (pixel counts).
// CTest invokes this as: scene_runner <path/to/x.scene>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

namespace {

struct Scene {
    int width = 32;
    int height = 32;
    bool msaa = false;
    uint32_t clear_rgba = 0;
    std::vector<gpu::Vec4f> positions;
    std::vector<gpu::Vec4f> colours;
    int expect_min_white = -1;
    int expect_edge_min  = -1;
};

bool parse_scene(const std::string& path, Scene& s, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open " + path; return false; }
    std::string line;
    bool in_verts = false;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        if (in_verts) {
            if (line.rfind("end", 0) == 0) { in_verts = false; continue; }
            std::istringstream is(line);
            float x, y, z, w, r, g, b, a;
            if (!(is >> x >> y >> z >> w >> r >> g >> b >> a)) {
                err = "bad vertex line"; return false;
            }
            s.positions.push_back({{x, y, z, w}});
            s.colours.push_back({{r, g, b, a}});
            continue;
        }
        std::istringstream is(line);
        std::string key; is >> key;
        if (key == "width")  is >> s.width;
        else if (key == "height") is >> s.height;
        else if (key == "msaa")   { int v; is >> v; s.msaa = v != 0; }
        else if (key == "clear")  { uint32_t v; is >> std::hex >> v >> std::dec; s.clear_rgba = v; }
        else if (key == "verts")  in_verts = true;
        else if (key == "expect_min_white") is >> s.expect_min_white;
        else if (key == "expect_edge_min")  is >> s.expect_edge_min;
        else { err = "unknown key '" + key + "'"; return false; }
    }
    if (s.positions.size() % 3 != 0 || s.positions.empty()) {
        err = "vertex count must be a positive multiple of 3";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <scene.scene>\n", argv[0]);
        return 2;
    }
    Scene scene;
    std::string err;
    if (!parse_scene(argv[1], scene, err)) {
        std::fprintf(stderr, "parse: %s\n", err.c_str());
        return 1;
    }

    auto vs = gpu::asm_::assemble("mov o0, r0\nmov o1, r1\n");
    auto fs = gpu::asm_::assemble("mov o0, v0\n");
    if (!vs.error.empty() || !fs.error.empty()) {
        std::fprintf(stderr, "asm err\n"); return 1;
    }
    const std::vector<uint64_t> vs_bin(vs.code.begin(), vs.code.end());
    const std::vector<uint64_t> fs_bin(fs.code.begin(), fs.code.end());

    gpu::Context ctx{};
    ctx.fb.width  = scene.width;
    ctx.fb.height = scene.height;
    ctx.fb.color.assign(scene.width * scene.height, scene.clear_rgba);
    if (scene.msaa) {
        ctx.fb.msaa_4x = true;
        ctx.fb.color_samples.assign(scene.width * scene.height * 4, scene.clear_rgba);
    }
    ctx.draw.vp_w = scene.width;
    ctx.draw.vp_h = scene.height;
    ctx.draw.primitive = gpu::DrawState::TRIANGLES;
    ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, scene.positions.data()};
    ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, scene.colours.data()};
    ctx.shaders.vs_binary = &vs_bin;
    ctx.shaders.fs_binary = &fs_bin;
    ctx.shaders.vs_attr_count = 2;
    ctx.shaders.vs_varying_count = 1;
    ctx.shaders.fs_varying_count = 1;

    gpu::pipeline::draw(ctx,
                       static_cast<uint32_t>(scene.positions.size()));

    int white = 0, edge = 0;
    for (uint32_t c : ctx.fb.color) {
        const uint8_t r = c & 0xFF;
        const uint8_t g = (c >> 8) & 0xFF;
        const uint8_t b = (c >> 16) & 0xFF;
        if (r >= 240 && g >= 240 && b >= 240) ++white;
        else if (r > 0 && r < 240) ++edge;
    }

    int fails = 0;
    if (scene.expect_min_white >= 0 && white < scene.expect_min_white) {
        std::fprintf(stderr, "FAIL: white=%d < expect_min_white=%d\n",
                     white, scene.expect_min_white);
        ++fails;
    }
    if (scene.expect_edge_min >= 0 && edge < scene.expect_edge_min) {
        std::fprintf(stderr, "FAIL: edge=%d < expect_edge_min=%d\n",
                     edge, scene.expect_edge_min);
        ++fails;
    }
    if (fails) return 1;
    std::printf("PASS %s — white=%d edge=%d\n", argv[1], white, edge);
    return 0;
}
