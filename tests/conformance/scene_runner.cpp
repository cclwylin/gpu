// Sprint 11 / 15 conformance harness.
//
// Reads a tiny line-oriented .scene file, runs it through sw_ref using a
// pass-through VS+FS, then asserts pixel-count expectations and (Sprint 15)
// optionally diff against a golden PPM with an RMSE budget.
//
// Usage:
//   scene_runner <scene>                 # run + assert
//   scene_runner <scene> --write-golden  # write the rendered FB to the
//                                          scene's golden_ppm path

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    int    expect_min_white = -1;
    int    expect_edge_min  = -1;
    std::string golden_ppm;          // path relative to scene file
    float  expect_rmse_max  = -1.0f;
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
        if      (key == "width")  is >> s.width;
        else if (key == "height") is >> s.height;
        else if (key == "msaa")   { int v; is >> v; s.msaa = v != 0; }
        else if (key == "clear")  { uint32_t v; is >> std::hex >> v >> std::dec; s.clear_rgba = v; }
        else if (key == "verts")  in_verts = true;
        else if (key == "expect_min_white") is >> s.expect_min_white;
        else if (key == "expect_edge_min")  is >> s.expect_edge_min;
        else if (key == "golden_ppm")       is >> s.golden_ppm;
        else if (key == "expect_rmse_max")  is >> s.expect_rmse_max;
        else { err = "unknown key '" + key + "'"; return false; }
    }
    if (s.positions.size() % 3 != 0 || s.positions.empty()) {
        err = "vertex count must be a positive multiple of 3";
        return false;
    }
    return true;
}

std::string dir_of(const std::string& path) {
    auto p = path.find_last_of('/');
    return p == std::string::npos ? std::string(".") : path.substr(0, p);
}

bool write_ppm(const std::string& path, const std::vector<uint32_t>& fb,
               int W, int H) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << W << " " << H << "\n255\n";
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            uint32_t c = fb[static_cast<size_t>(y) * W + x];
            f.put(static_cast<char>(c & 0xFF));
            f.put(static_cast<char>((c >>  8) & 0xFF));
            f.put(static_cast<char>((c >> 16) & 0xFF));
        }
    }
    return true;
}

bool read_ppm(const std::string& path, std::vector<uint32_t>& out,
              int& W, int& H, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open " + path; return false; }
    std::string magic;
    int maxv = 0;
    f >> magic >> W >> H >> maxv;
    if (magic != "P6" || maxv != 255) {
        err = "unsupported PPM format (need P6 maxval 255)"; return false;
    }
    f.get();     // consume single whitespace before binary block
    out.assign(static_cast<size_t>(W) * H, 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            unsigned char rgb[3];
            f.read(reinterpret_cast<char*>(rgb), 3);
            if (!f) { err = "PPM truncated"; return false; }
            uint32_t v = static_cast<uint32_t>(rgb[0])
                       | (static_cast<uint32_t>(rgb[1]) <<  8)
                       | (static_cast<uint32_t>(rgb[2]) << 16)
                       | (0xFFu << 24);
            out[static_cast<size_t>(y) * W + x] = v;
        }
    }
    return true;
}

float rmse(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return 1e9f;
    double sum_sq = 0.0;
    size_t N = a.size();
    for (size_t i = 0; i < N; ++i) {
        for (int ch = 0; ch < 3; ++ch) {
            float av = static_cast<float>((a[i] >> (ch * 8)) & 0xFF);
            float bv = static_cast<float>((b[i] >> (ch * 8)) & 0xFF);
            sum_sq += static_cast<double>((av - bv) * (av - bv));
        }
    }
    return static_cast<float>(std::sqrt(sum_sq / static_cast<double>(N * 3)));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <scene.scene> [--write-golden]\n", argv[0]);
        return 2;
    }
    const std::string scene_path = argv[1];
    bool write_golden = (argc >= 3 && std::strcmp(argv[2], "--write-golden") == 0);

    Scene scene;
    std::string err;
    if (!parse_scene(scene_path, scene, err)) {
        std::fprintf(stderr, "parse: %s\n", err.c_str()); return 1;
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
    ctx.shaders.vs_attr_count    = 2;
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

    if (write_golden) {
        if (scene.golden_ppm.empty()) {
            std::fprintf(stderr, "scene has no golden_ppm; nothing to write\n");
            return 1;
        }
        const std::string out_path = dir_of(scene_path) + "/" + scene.golden_ppm;
        if (!write_ppm(out_path, ctx.fb.color, scene.width, scene.height)) {
            std::fprintf(stderr, "FAIL: cannot write %s\n", out_path.c_str()); return 1;
        }
        std::printf("wrote %s\n", out_path.c_str());
        return 0;
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
    if (!scene.golden_ppm.empty() && scene.expect_rmse_max >= 0.0f) {
        const std::string golden_path = dir_of(scene_path) + "/" + scene.golden_ppm;
        std::vector<uint32_t> golden;
        int gw = 0, gh = 0;
        std::string read_err;
        if (!read_ppm(golden_path, golden, gw, gh, read_err)) {
            std::fprintf(stderr, "FAIL: golden read: %s\n", read_err.c_str()); ++fails;
        } else if (gw != scene.width || gh != scene.height) {
            std::fprintf(stderr, "FAIL: golden size %dx%d != scene %dx%d\n",
                         gw, gh, scene.width, scene.height); ++fails;
        } else {
            float r = rmse(ctx.fb.color, golden);
            std::printf("rmse vs golden = %g (max %g)\n", r, scene.expect_rmse_max);
            if (r > scene.expect_rmse_max) {
                std::fprintf(stderr, "FAIL: rmse %g exceeds %g\n",
                             r, scene.expect_rmse_max); ++fails;
            }
        }
    }
    if (fails) return 1;
    std::printf("PASS %s — white=%d edge=%d\n", argv[1], white, edge);
    return 0;
}
