// 4× MSAA: render the same triangle in 1× and 4× and assert that the
// resolved framebuffer differs along the silhouette edge by way of partial
// coverage producing intermediate (averaged) pixel values.
//
// We don't bit-compare the whole FB — that's a rendering-equivalence test for
// later. The minimum test here is: with MSAA on, edge pixels exist that are
// neither fully background nor fully foreground.

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

    // White triangle on black background.
    static const std::array<gpu::Vec4f, 3> positions = {
        gpu::Vec4f{{ 0.0f,  0.7f, 0.0f, 1.0f}},
        gpu::Vec4f{{-0.7f, -0.7f, 0.0f, 1.0f}},
        gpu::Vec4f{{ 0.7f, -0.7f, 0.0f, 1.0f}},
    };
    static const std::array<gpu::Vec4f, 3> colours = {
        gpu::Vec4f{{1.0f, 1.0f, 1.0f, 1.0f}},
        gpu::Vec4f{{1.0f, 1.0f, 1.0f, 1.0f}},
        gpu::Vec4f{{1.0f, 1.0f, 1.0f, 1.0f}},
    };

    auto setup = [&](gpu::Context& ctx, bool msaa) {
        ctx.fb.width = W; ctx.fb.height = H;
        ctx.fb.msaa_4x = msaa;
        ctx.fb.color.assign(W * H, 0u);
        if (msaa) ctx.fb.color_samples.assign(W * H * 4, 0u);
        ctx.draw.vp_w = W; ctx.draw.vp_h = H;
        ctx.draw.primitive = gpu::DrawState::TRIANGLES;
        ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, positions.data()};
        ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, colours.data()};
        ctx.shaders.vs_binary = &vs_bin;
        ctx.shaders.fs_binary = &fs_bin;
        ctx.shaders.vs_attr_count = 2;
        ctx.shaders.vs_varying_count = 1;
        ctx.shaders.fs_varying_count = 1;
    };

    gpu::Context ctx_1x{}, ctx_4x{};
    setup(ctx_1x, false);
    setup(ctx_4x, true);

    gpu::pipeline::draw(ctx_1x, 3);
    gpu::pipeline::draw(ctx_4x, 3);

    // Count pixels by category for each FB.
    auto categorise = [&](const gpu::Framebuffer& fb) {
        int bg = 0, fg = 0, edge = 0;
        for (uint32_t c : fb.color) {
            const uint32_t r = c & 0xFF;
            if (r == 0)        ++bg;
            else if (r == 255) ++fg;
            else               ++edge;
        }
        return std::array<int, 3>{bg, fg, edge};
    };
    auto a = categorise(ctx_1x.fb);
    auto b = categorise(ctx_4x.fb);

    std::printf("1x : bg=%d fg=%d edge=%d\n", a[0], a[1], a[2]);
    std::printf("4x : bg=%d fg=%d edge=%d\n", b[0], b[1], b[2]);

    // Sanity 1: 1× has zero or near-zero edge pixels (binary coverage).
    if (a[2] > 4) {
        std::fprintf(stderr, "FAIL: 1x has too many edge pixels (%d)\n", a[2]);
        return 1;
    }
    // Sanity 2: 4× has many edge pixels (partial coverage produced grey).
    if (b[2] < 10) {
        std::fprintf(stderr, "FAIL: 4x has too few edge pixels (%d) — resolve broken\n",
                     b[2]);
        return 1;
    }
    // Sanity 3: 4× still has a foreground core.
    if (b[1] < 50) {
        std::fprintf(stderr, "FAIL: 4x has too few foreground pixels (%d)\n", b[1]);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
