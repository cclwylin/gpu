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

// Sprint 40 — multi-batch scenes. Each batch carries its own render
// state snapshot so glcompat captures of programs that toggle depth/
// blend mid-frame (movelight torus + bitmap-font HUD, etc.) replay
// faithfully. Legacy single-state scenes parse as one implicit batch.
struct Batch {
    bool   depth_test  = false;
    bool   depth_write = true;
    std::string depth_func = "less";
    bool   cull_back  = false;
    bool   blend      = false;
    std::vector<gpu::Vec4f> positions;
    std::vector<gpu::Vec4f> colours;
};

struct Scene {
    int width = 32;
    int height = 32;
    bool msaa = false;
    uint32_t clear_rgba = 0;
    std::vector<Batch> batches;
    int    expect_min_white = -1;
    int    expect_edge_min  = -1;
    std::string golden_ppm;          // path relative to scene file
    float  expect_rmse_max  = -1.0f;
};

bool parse_scene(const std::string& path, Scene& s, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open " + path; return false; }
    Batch defaults;
    Batch* cur = nullptr;
    Batch  legacy;
    bool   has_legacy_verts = false;
    bool   in_verts = false;
    std::string line;
    while (std::getline(f, line)) {
        const size_t fnw = line.find_first_not_of(" \t");
        if (fnw == std::string::npos || line[fnw] == '#') continue;
        if (in_verts) {
            std::istringstream is(line);
            std::string first; is >> first;
            if (first == "end" || first == "end_verts") { in_verts = false; continue; }
            std::istringstream is2(line);
            float x, y, z, w, r, g, b, a;
            if (!(is2 >> x >> y >> z >> w >> r >> g >> b >> a)) {
                err = "bad vertex line: " + line; return false;
            }
            Batch* tgt = cur ? cur : (has_legacy_verts ? &legacy : (legacy = defaults, has_legacy_verts = true, &legacy));
            tgt->positions.push_back({{x, y, z, w}});
            tgt->colours.push_back({{r, g, b, a}});
            continue;
        }
        std::istringstream is(line);
        std::string key; is >> key;
        if (key == "batch") {
            s.batches.emplace_back(defaults);
            cur = &s.batches.back();
            continue;
        }
        if (key == "end_batch") { cur = nullptr; continue; }
        if (key == "width")  { is >> s.width;  continue; }
        if (key == "height") { is >> s.height; continue; }
        if (key == "msaa")   { int v; is >> v; s.msaa = (v != 0); continue; }
        if (key == "clear")  { uint32_t v; is >> std::hex >> v >> std::dec; s.clear_rgba = v; continue; }
        if (key == "expect_min_white") { is >> s.expect_min_white; continue; }
        if (key == "expect_edge_min")  { is >> s.expect_edge_min;  continue; }
        if (key == "golden_ppm")       { is >> s.golden_ppm;       continue; }
        if (key == "expect_rmse_max")  { is >> s.expect_rmse_max;  continue; }
        Batch* tgt = cur ? cur : &defaults;
        if      (key == "depth_test")  { int v; is >> v; tgt->depth_test  = v != 0; }
        else if (key == "depth_write") { int v; is >> v; tgt->depth_write = v != 0; }
        else if (key == "depth_func")  { is >> tgt->depth_func; }
        else if (key == "cull_back")   { int v; is >> v; tgt->cull_back   = v != 0; }
        else if (key == "blend")       { int v; is >> v; tgt->blend       = v != 0; }
        else if (key == "verts")       { in_verts = true; }
        else { err = "unknown key '" + key + "'"; return false; }
    }
    if (has_legacy_verts) s.batches.push_back(std::move(legacy));
    for (const auto& b : s.batches) {
        if (b.positions.size() % 3 != 0) {
            err = "batch vertex count must be a multiple of 3";
            return false;
        }
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
        std::fprintf(stderr,
                     "usage: %s <scene.scene> [--write-golden | --out <ppm>]\n",
                     argv[0]);
        return 2;
    }
    const std::string scene_path = argv[1];
    bool write_golden = false;
    std::string out_ppm;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--write-golden") == 0) {
            write_golden = true;
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_ppm = argv[++i];
        }
    }

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
    bool any_depth_test = false;
    for (const auto& b : scene.batches)
        if (b.depth_test) { any_depth_test = true; break; }
    if (any_depth_test)
        ctx.fb.depth.assign((size_t)scene.width * scene.height, 1.0f);
    ctx.draw.vp_w = scene.width;
    ctx.draw.vp_h = scene.height;
    ctx.draw.primitive = gpu::DrawState::TRIANGLES;
    ctx.shaders.vs_binary = &vs_bin;
    ctx.shaders.fs_binary = &fs_bin;
    ctx.shaders.vs_attr_count    = 2;
    ctx.shaders.vs_varying_count = 1;
    ctx.shaders.fs_varying_count = 1;
    auto apply_batch = [&](const Batch& b) {
        ctx.draw.depth_test  = b.depth_test;
        ctx.draw.depth_write = b.depth_write;
        ctx.draw.cull_back   = b.cull_back;
        ctx.draw.blend_enable = b.blend;
        using DF = gpu::DrawState;
        if      (b.depth_func == "never")    ctx.draw.depth_func = DF::DF_NEVER;
        else if (b.depth_func == "less")     ctx.draw.depth_func = DF::DF_LESS;
        else if (b.depth_func == "lequal")   ctx.draw.depth_func = DF::DF_LEQUAL;
        else if (b.depth_func == "equal")    ctx.draw.depth_func = DF::DF_EQUAL;
        else if (b.depth_func == "gequal")   ctx.draw.depth_func = DF::DF_GEQUAL;
        else if (b.depth_func == "greater")  ctx.draw.depth_func = DF::DF_GREATER;
        else if (b.depth_func == "notequal") ctx.draw.depth_func = DF::DF_NOTEQUAL;
        else if (b.depth_func == "always")   ctx.draw.depth_func = DF::DF_ALWAYS;
    };
    size_t total_verts = 0;
    for (const auto& batch : scene.batches) {
        if (batch.positions.empty()) continue;
        apply_batch(batch);
        ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, batch.positions.data()};
        ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, batch.colours.data()};
        gpu::pipeline::draw(ctx,
                           static_cast<uint32_t>(batch.positions.size()));
        total_verts += batch.positions.size();
    }

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

    if (!out_ppm.empty()) {
        if (!write_ppm(out_ppm, ctx.fb.color, scene.width, scene.height)) {
            std::fprintf(stderr, "FAIL: cannot write %s\n", out_ppm.c_str());
            return 1;
        }
        std::printf("PPM=%s\n", out_ppm.c_str());
        std::printf("PAINTED=%d\n", white + edge);
        std::printf("TRIANGLES=%zu\n", total_verts / 3);
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
