// Bind a 4×4 RGB checker texture, draw a fullscreen triangle whose UVs span
// [0,1]×[0,1], and verify that center pixels contain texture-derived colours.

#include <array>
#include <cstdio>
#include <vector>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/texture.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

namespace {

// VS: pass-through pos as o0 (clip space), uv via varying o1
const char* kVS =
    "mov o0, r0\n"
    "mov o1, r1\n";

// FS: gl_FragColor = texture2D(tex0, v0.xy)
// ISA v1.1: MEM format gained src_class + dst_class so the Sprint-4
// `mov rN, vN; tex; mov oN, rN` workaround collapses to one instruction.
const char* kFS =
    "tex o0, v0.xy, tex0\n";

uint32_t pack(uint8_t r, uint8_t g, uint8_t b) {
    return r | (g << 8) | (b << 16) | (0xFFu << 24);
}

}  // namespace

int main() {
    constexpr int W = 64, H = 64;

    // 2×2 quadrant texture: TL red, TR green, BL blue, BR yellow.
    gpu::Texture tex;
    tex.format = gpu::Texture::RGBA8;
    tex.filter = gpu::Texture::NEAREST;
    tex.wrap_s = tex.wrap_t = gpu::Texture::CLAMP;
    tex.width = 2; tex.height = 2;
    tex.texels = {
        pack(255,   0,   0),   // (0,0) red
        pack(  0, 255,   0),   // (1,0) green
        pack(  0,   0, 255),   // (0,1) blue
        pack(255, 255,   0),   // (1,1) yellow
    };

    auto vs = gpu::asm_::assemble(kVS);
    auto fs = gpu::asm_::assemble(kFS);
    if (!vs.error.empty() || !fs.error.empty()) {
        std::fprintf(stderr, "asm err vs=%s fs=%s\n",
                     vs.error.c_str(), fs.error.c_str()); return 1;
    }
    const std::vector<uint64_t> vs_bin(vs.code.begin(), vs.code.end());
    const std::vector<uint64_t> fs_bin(fs.code.begin(), fs.code.end());

    // Big screen-covering triangle so each quadrant of UV [0,1]² maps to many pixels.
    static const std::array<gpu::Vec4f, 3> positions = {
        gpu::Vec4f{{-3.0f, -1.0f, 0.0f, 1.0f}},
        gpu::Vec4f{{ 1.0f, -1.0f, 0.0f, 1.0f}},
        gpu::Vec4f{{ 1.0f,  3.0f, 0.0f, 1.0f}},
    };
    // Match UVs to those NDC positions so screen sweep ≈ uv sweep.
    static const std::array<gpu::Vec4f, 3> uvs = {
        gpu::Vec4f{{-1.0f, 0.0f, 0, 0}},
        gpu::Vec4f{{ 1.0f, 0.0f, 0, 0}},
        gpu::Vec4f{{ 1.0f, 2.0f, 0, 0}},
    };

    gpu::Context ctx{};
    ctx.fb.width = W; ctx.fb.height = H;
    ctx.fb.color.assign(W * H, 0u);
    ctx.draw.vp_w = W; ctx.draw.vp_h = H;
    ctx.draw.primitive = gpu::DrawState::TRIANGLES;
    ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, positions.data()};
    ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, uvs.data()};
    ctx.shaders.vs_binary = &vs_bin;
    ctx.shaders.fs_binary = &fs_bin;
    ctx.shaders.vs_attr_count = 2;
    ctx.shaders.vs_varying_count = 1;
    ctx.shaders.fs_varying_count = 1;
    ctx.textures[0] = &tex;

    gpu::pipeline::draw(ctx, 3);

    // Sample at four pixels expected to lie in different quadrants.
    auto pix = [&](int x, int y) -> uint32_t {
        return ctx.fb.color[static_cast<size_t>(y) * W + x];
    };
    auto rgb = [](uint32_t p, int& r, int& g, int& b) {
        r = (p >>  0) & 0xFF;
        g = (p >>  8) & 0xFF;
        b = (p >> 16) & 0xFF;
    };

    int painted = 0;
    int red = 0, green = 0, blue = 0, yellow = 0;
    for (uint32_t p : ctx.fb.color) {
        if (p == 0) continue;
        ++painted;
        int r, g, b; rgb(p, r, g, b);
        if (r == 255 && g ==   0 && b ==   0) ++red;
        if (r ==   0 && g == 255 && b ==   0) ++green;
        if (r ==   0 && g ==   0 && b == 255) ++blue;
        if (r == 255 && g == 255 && b ==   0) ++yellow;
    }

    std::printf("painted=%d red=%d green=%d blue=%d yellow=%d\n",
                painted, red, green, blue, yellow);
    std::printf("center pixel (W/2, H/2) = 0x%08x\n", pix(W/2, H/2));

    int fails = 0;
    if (red    < 50) { std::fprintf(stderr, "FAIL: red quadrant short (%d)\n", red); ++fails; }
    if (green  < 50) { std::fprintf(stderr, "FAIL: green quadrant short (%d)\n", green); ++fails; }
    if (blue   < 50) { std::fprintf(stderr, "FAIL: blue quadrant short (%d)\n", blue); ++fails; }
    if (yellow < 50) { std::fprintf(stderr, "FAIL: yellow quadrant short (%d)\n", yellow); ++fails; }
    if (fails) return 1;

    std::printf("PASS\n");
    return 0;
}
