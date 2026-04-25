// End-to-end smoke: same triangle as main_smoke, asserted via CTest.
#include <array>
#include <cassert>
#include <cstdio>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"

namespace {
void vs(const gpu::DrawState&, const gpu::Vec4f* attrs, gpu::Vertex& out) {
    out.pos = attrs[0];
    out.varying[0] = attrs[1];
    out.varying_count = 1;
}
void fs(const gpu::DrawState&, const gpu::Vec4f* varying, gpu::Vec4f& out) {
    out = varying[0];
}
}  // namespace

int main() {
    constexpr int W = 32, H = 32;
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
    ctx.shaders.vs = vs;
    ctx.shaders.fs = fs;

    gpu::pipeline::draw(ctx, 3);

    int painted = 0;
    for (uint32_t c : ctx.fb.color) if (c != 0) ++painted;

    // 32x32 viewport with a triangle covering ~half the screen.
    // Skeleton tolerance: at least 100 pixels rasterised.
    if (painted < 100) {
        std::fprintf(stderr, "FAIL: painted=%d (expected >=100)\n", painted);
        return 1;
    }
    std::printf("PASS: painted=%d\n", painted);
    return 0;
}
