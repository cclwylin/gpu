// PFO depth + blend smoke.
//   1) Depth test: two overlapping triangles, the back one only paints where
//      the front does NOT cover.
//   2) Alpha blend: SRC_ALPHA / ONE_MINUS_SRC_ALPHA against a solid background.

#include <array>
#include <cstdio>
#include <vector>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

namespace {
const char* kVS = "mov o0, r0\nmov o1, r1\n";
const char* kFS = "mov o0, v0\n";

uint32_t pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return r | (g << 8) | (b << 16) | (a << 24);
}
}

int main() {
    constexpr int W = 32, H = 32;

    auto vs = gpu::asm_::assemble(kVS);
    auto fs = gpu::asm_::assemble(kFS);
    if (!vs.error.empty() || !fs.error.empty()) {
        std::fprintf(stderr, "asm err\n"); return 1;
    }
    const std::vector<uint64_t> vs_bin(vs.code.begin(), vs.code.end());
    const std::vector<uint64_t> fs_bin(fs.code.begin(), fs.code.end());

    auto setup_ctx = [&](gpu::Context& ctx) {
        ctx.fb.width = W; ctx.fb.height = H;
        ctx.fb.color.assign(W * H, 0u);
        ctx.fb.depth.assign(W * H, 1.0f);
        ctx.draw.vp_w = W; ctx.draw.vp_h = H;
        ctx.draw.primitive = gpu::DrawState::TRIANGLES;
        ctx.shaders.vs_binary = &vs_bin;
        ctx.shaders.fs_binary = &fs_bin;
        ctx.shaders.vs_attr_count = 2;
        ctx.shaders.vs_varying_count = 1;
        ctx.shaders.fs_varying_count = 1;
    };

    // ---------------- 1) Depth test ----------------
    {
        gpu::Context ctx{};
        setup_ctx(ctx);
        ctx.draw.depth_test = true;
        ctx.draw.depth_func = gpu::DrawState::DF_LESS;
        ctx.draw.depth_write = true;

        // Front triangle (depth 0.2, red, top-left half)
        std::array<gpu::Vec4f, 3> front_pos = {
            gpu::Vec4f{{-0.9f,  0.9f, 0.2f, 1.0f}},
            gpu::Vec4f{{-0.9f, -0.9f, 0.2f, 1.0f}},
            gpu::Vec4f{{ 0.9f,  0.9f, 0.2f, 1.0f}},
        };
        std::array<gpu::Vec4f, 3> front_col = {
            gpu::Vec4f{{1, 0, 0, 1}},
            gpu::Vec4f{{1, 0, 0, 1}},
            gpu::Vec4f{{1, 0, 0, 1}},
        };
        // Back triangle (depth 0.8, blue, full screen)
        std::array<gpu::Vec4f, 3> back_pos = {
            gpu::Vec4f{{-1.0f, -1.0f, 0.8f, 1.0f}},
            gpu::Vec4f{{ 1.0f, -1.0f, 0.8f, 1.0f}},
            gpu::Vec4f{{ 0.0f,  1.0f, 0.8f, 1.0f}},
        };
        std::array<gpu::Vec4f, 3> back_col = {
            gpu::Vec4f{{0, 0, 1, 1}},
            gpu::Vec4f{{0, 0, 1, 1}},
            gpu::Vec4f{{0, 0, 1, 1}},
        };

        // Draw front first
        ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32, sizeof(gpu::Vec4f), 0, front_pos.data()};
        ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32, sizeof(gpu::Vec4f), 0, front_col.data()};
        gpu::pipeline::draw(ctx, 3);

        // Draw back
        ctx.attribs[0].data = back_pos.data();
        ctx.attribs[1].data = back_col.data();
        gpu::pipeline::draw(ctx, 3);

        // Count colours
        int red = 0, blue = 0, bg = 0;
        for (uint32_t c : ctx.fb.color) {
            if (c == 0) ++bg;
            else if (((c >>  0) & 0xFF) == 255 && ((c >> 16) & 0xFF) == 0) ++red;
            else if (((c >> 16) & 0xFF) == 255 && ((c >>  0) & 0xFF) == 0) ++blue;
        }
        std::printf("[depth] red=%d blue=%d bg=%d\n", red, blue, bg);
        if (red == 0)  { std::fprintf(stderr, "FAIL: no front-tri pixels\n"); return 1; }
        if (blue == 0) { std::fprintf(stderr, "FAIL: no back-tri pixels (depth blocked everything?)\n"); return 1; }
        // Critically: back triangle must NOT have overwritten any red pixel.
        // We can't tell from counts alone, but we sample a known overlap point.
        const int cx = W / 4, cy = H / 4;
        const uint32_t mid = ctx.fb.color[cy * W + cx];
        const uint8_t r = mid & 0xFF;
        if (r != 255) {
            std::fprintf(stderr, "FAIL: depth test let back overwrite front at (%d,%d): 0x%08x\n",
                         cx, cy, mid);
            return 1;
        }
    }

    // ---------------- 2) Alpha blend ----------------
    {
        gpu::Context ctx{};
        setup_ctx(ctx);
        // Background = solid red (pre-fill the FB).
        for (auto& p : ctx.fb.color) p = pack(255, 0, 0, 255);

        ctx.draw.blend_enable = true;
        ctx.draw.blend_src = gpu::DrawState::BF_SRC_ALPHA;
        ctx.draw.blend_dst = gpu::DrawState::BF_ONE_MINUS_SRC_ALPHA;
        ctx.draw.blend_eq  = gpu::DrawState::BE_ADD;

        // 50%-alpha green triangle covering centre.
        std::array<gpu::Vec4f, 3> pos = {
            gpu::Vec4f{{-0.9f,  0.9f, 0.5f, 1.0f}},
            gpu::Vec4f{{-0.9f, -0.9f, 0.5f, 1.0f}},
            gpu::Vec4f{{ 0.9f,  0.0f, 0.5f, 1.0f}},
        };
        std::array<gpu::Vec4f, 3> col = {
            gpu::Vec4f{{0, 1, 0, 0.5f}},
            gpu::Vec4f{{0, 1, 0, 0.5f}},
            gpu::Vec4f{{0, 1, 0, 0.5f}},
        };
        ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32, sizeof(gpu::Vec4f), 0, pos.data()};
        ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32, sizeof(gpu::Vec4f), 0, col.data()};
        gpu::pipeline::draw(ctx, 3);

        // Count "muddy" pixels (R != 255, G != 255 — i.e. blended).
        int blended = 0, pure_red = 0, pure_green = 0;
        for (uint32_t c : ctx.fb.color) {
            uint8_t r = c & 0xFF, g = (c >> 8) & 0xFF;
            if (r == 255 && g ==   0) ++pure_red;
            else if (r ==   0 && g == 255) ++pure_green;
            else if (r >   0 && g >   0) ++blended;
        }
        std::printf("[blend] pure_red=%d blended=%d pure_green=%d\n",
                    pure_red, blended, pure_green);
        if (blended < 100) {
            std::fprintf(stderr, "FAIL: alpha blend produced no blended pixels\n");
            return 1;
        }
        // Centre of triangle should be ~ (red*0.5 + green*0.5) = (128, 128, 0).
        const int cx = W / 3, cy = H / 2;
        const uint32_t mid = ctx.fb.color[cy * W + cx];
        const uint8_t r = mid & 0xFF, g = (mid >> 8) & 0xFF;
        if (r < 100 || r > 200 || g < 100 || g > 200) {
            std::fprintf(stderr, "FAIL: blend midpoint %d,%d outside expected range\n", r, g);
            return 1;
        }
    }

    std::printf("PASS\n");
    return 0;
}
