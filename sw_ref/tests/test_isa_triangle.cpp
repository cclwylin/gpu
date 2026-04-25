// Hello-triangle through the real ISA simulator.
// Same workload as test_basic.cpp, but VS + FS are now compiled-from-asm
// and executed via compiler/isa_sim/. Validates the full glue.

#include <array>
#include <cstdio>
#include <vector>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

namespace {

// VS: pass-through pos as o0, pass-through colour as o1.
//   r0 = a_pos     (attribute slot 0)
//   r1 = a_colour  (attribute slot 1)
//   o0 = pos
//   o1 = varying colour
const char* kVS =
    "mov o0, r0\n"
    "mov o1, r1\n";

// FS: gl_FragColor = v0  (the interpolated colour)
const char* kFS =
    "mov o0, v0\n";

}  // namespace

int main() {
    constexpr int W = 32, H = 32;

    auto vs = gpu::asm_::assemble(kVS);
    auto fs = gpu::asm_::assemble(kFS);
    if (!vs.error.empty() || !fs.error.empty()) {
        std::fprintf(stderr, "asm error vs=%s fs=%s\n",
                     vs.error.c_str(), fs.error.c_str());
        return 1;
    }
    const std::vector<uint64_t> vs_bin(vs.code.begin(), vs.code.end());
    const std::vector<uint64_t> fs_bin(fs.code.begin(), fs.code.end());

    static const std::array<gpu::Vec4f, 3> positions = {
        gpu::Vec4f{{ 0.0f,  0.9f, 0.0f, 1.0f}},
        gpu::Vec4f{{-0.9f, -0.9f, 0.0f, 1.0f}},
        gpu::Vec4f{{ 0.9f, -0.9f, 0.0f, 1.0f}},
    };
    static const std::array<gpu::Vec4f, 3> colours = {
        gpu::Vec4f{{1.0f, 1.0f, 1.0f, 1.0f}},
        gpu::Vec4f{{1.0f, 1.0f, 1.0f, 1.0f}},
        gpu::Vec4f{{1.0f, 1.0f, 1.0f, 1.0f}},
    };

    gpu::Context ctx{};
    ctx.fb.width = W; ctx.fb.height = H;
    ctx.fb.color.assign(W * H, 0u);
    ctx.draw.vp_w = W; ctx.draw.vp_h = H;
    ctx.draw.primitive = gpu::DrawState::TRIANGLES;
    ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, positions.data()};
    ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, colours.data()};

    ctx.shaders.vs_binary = &vs_bin;
    ctx.shaders.fs_binary = &fs_bin;
    ctx.shaders.vs_attr_count    = 2;
    ctx.shaders.vs_varying_count = 1;     // o1 -> v0
    ctx.shaders.fs_varying_count = 1;

    gpu::pipeline::draw(ctx, 3);

    int painted = 0;
    for (uint32_t c : ctx.fb.color) if (c != 0) ++painted;
    if (painted < 100) {
        std::fprintf(stderr, "FAIL: painted=%d (expected >=100)\n", painted);
        return 1;
    }
    std::printf("PASS: painted=%d (ISA-driven)\n", painted);
    return 0;
}
