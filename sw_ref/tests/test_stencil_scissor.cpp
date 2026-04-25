// Sprint 17: stencil + scissor smoke.
//
// Test 1 (scissor): render a fullscreen white triangle with scissor box
// 8..23 × 8..23. Expect zero painted pixels outside the box.
//
// Test 2 (stencil): two-pass.
//   Pass A: write-stencil-to-1 in a 16x16 region (front triangle covers it).
//           color writes disabled by setting blend ZERO/ONE? Simpler: clear FB,
//           write stencil only by using DF_NEVER on depth for shading control.
//           For Sprint 17 we just allow color to draw too — the goal is to
//           verify stencil routing.
//   Pass B: full-screen triangle, stencil_test = SF_EQUAL, ref=1.
//           Only pixels where stencil==1 should paint.

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

    auto setup = [&](gpu::Context& ctx) {
        ctx.fb.width = W; ctx.fb.height = H;
        ctx.fb.color.assign(W * H, 0u);
        ctx.fb.depth.assign(W * H, 1.0f);
        ctx.fb.stencil.assign(W * H, 0u);
        ctx.draw.vp_w = W; ctx.draw.vp_h = H;
        ctx.draw.primitive = gpu::DrawState::TRIANGLES;
        ctx.shaders.vs_binary = &vs_bin;
        ctx.shaders.fs_binary = &fs_bin;
        ctx.shaders.vs_attr_count = 2;
        ctx.shaders.vs_varying_count = 1;
        ctx.shaders.fs_varying_count = 1;
    };

    int fails = 0;

    // ---------------- 1) Scissor ----------------
    {
        gpu::Context ctx{};
        setup(ctx);
        ctx.draw.scissor_enable = true;
        ctx.draw.scissor_x = 8; ctx.draw.scissor_y = 8;
        ctx.draw.scissor_w = 16; ctx.draw.scissor_h = 16;

        std::array<gpu::Vec4f, 3> pos = {
            gpu::Vec4f{{-1.0f, -1.0f, 0.5f, 1.0f}},
            gpu::Vec4f{{ 3.0f, -1.0f, 0.5f, 1.0f}},
            gpu::Vec4f{{-1.0f,  3.0f, 0.5f, 1.0f}},
        };
        std::array<gpu::Vec4f, 3> col = {
            gpu::Vec4f{{1, 1, 1, 1}}, gpu::Vec4f{{1, 1, 1, 1}}, gpu::Vec4f{{1, 1, 1, 1}},
        };
        ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, pos.data()};
        ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, col.data()};
        gpu::pipeline::draw(ctx, 3);

        int painted = 0, outside = 0;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const uint32_t c = ctx.fb.color[y * W + x];
                if (c == 0) continue;
                ++painted;
                bool inside = (x >= 8 && x < 24 && y >= 8 && y < 24);
                if (!inside) ++outside;
            }
        }
        std::printf("[scissor] painted=%d outside=%d\n", painted, outside);
        if (painted < 100) { std::fprintf(stderr, "FAIL: scissor too few painted (%d)\n", painted); ++fails; }
        if (outside > 0)   { std::fprintf(stderr, "FAIL: scissor leaked %d px outside box\n", outside); ++fails; }
    }

    // ---------------- 2) Stencil ----------------
    {
        gpu::Context ctx{};
        setup(ctx);

        // Pass A: write stencil = 1 wherever the front triangle covers.
        ctx.draw.stencil_test = true;
        ctx.draw.stencil_func = gpu::DrawState::SF_ALWAYS;
        ctx.draw.sop_zpass    = gpu::DrawState::SO_REPLACE;
        ctx.draw.sop_zfail    = gpu::DrawState::SO_KEEP;
        ctx.draw.sop_fail     = gpu::DrawState::SO_KEEP;
        ctx.draw.stencil_ref  = 1;
        ctx.draw.stencil_write_mask = 0xFF;
        ctx.draw.depth_test = false;     // not interested in depth here

        std::array<gpu::Vec4f, 3> pos_a = {
            gpu::Vec4f{{-0.5f,  0.5f, 0.5f, 1.0f}},
            gpu::Vec4f{{-0.5f, -0.5f, 0.5f, 1.0f}},
            gpu::Vec4f{{ 0.5f,  0.5f, 0.5f, 1.0f}},
        };
        std::array<gpu::Vec4f, 3> col_a = {
            gpu::Vec4f{{1, 0, 0, 1}}, gpu::Vec4f{{1, 0, 0, 1}}, gpu::Vec4f{{1, 0, 0, 1}},
        };
        ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, pos_a.data()};
        ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                          sizeof(gpu::Vec4f), 0, col_a.data()};
        gpu::pipeline::draw(ctx, 3);

        int stencil_one = 0;
        for (uint8_t v : ctx.fb.stencil) if (v == 1) ++stencil_one;
        if (stencil_one < 30) {
            std::fprintf(stderr, "FAIL: stencil pass A wrote only %d ones\n", stencil_one);
            ++fails;
        }

        // Pass B: full-screen blue triangle, only paint where stencil == 1.
        ctx.draw.stencil_test = true;
        ctx.draw.stencil_func = gpu::DrawState::SF_EQUAL;
        ctx.draw.sop_zpass    = gpu::DrawState::SO_KEEP;
        ctx.draw.stencil_ref  = 1;
        ctx.draw.stencil_read_mask = 0xFF;

        // Reset color so we can see only Pass B's contribution.
        ctx.fb.color.assign(W * H, 0u);
        std::array<gpu::Vec4f, 3> pos_b = {
            gpu::Vec4f{{-1.0f, -1.0f, 0.5f, 1.0f}},
            gpu::Vec4f{{ 3.0f, -1.0f, 0.5f, 1.0f}},
            gpu::Vec4f{{-1.0f,  3.0f, 0.5f, 1.0f}},
        };
        std::array<gpu::Vec4f, 3> col_b = {
            gpu::Vec4f{{0, 0, 1, 1}}, gpu::Vec4f{{0, 0, 1, 1}}, gpu::Vec4f{{0, 0, 1, 1}},
        };
        ctx.attribs[0].data = pos_b.data();
        ctx.attribs[1].data = col_b.data();
        gpu::pipeline::draw(ctx, 3);

        int blue_painted = 0;
        for (uint32_t c : ctx.fb.color) {
            if (c == 0) continue;
            ++blue_painted;
        }
        std::printf("[stencil] stencil_ones=%d painted_in_passB=%d\n",
                    stencil_one, blue_painted);
        if (blue_painted == 0) {
            std::fprintf(stderr, "FAIL: stencil pass B produced no pixels\n"); ++fails;
        }
        if (blue_painted > stencil_one + 50) {
            // pass B should only paint inside the stencil-1 region (small slop)
            std::fprintf(stderr, "FAIL: stencil pass B painted too widely (%d > %d)\n",
                         blue_painted, stencil_one);
            ++fails;
        }
    }

    if (fails) return 1;
    std::printf("PASS\n");
    return 0;
}
