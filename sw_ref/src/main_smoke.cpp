// Hello-triangle smoke for the sw_ref skeleton.
//
// Builds a minimal Context, binds two C++ functor "shaders", draws one
// triangle into a 64x64 RGBA8 framebuffer, and writes a PPM. Verifies the
// pipeline links and the chosen pixels are non-background.
//
// Phase 1 will replace the C++ functors with the GLSL interpreter; right now
// we are only proving build + plumbing.

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"

namespace {

// Toy VS: output position straight from attr0; pass attr1 to varying[0].
void vs_passthrough(const gpu::DrawState&, const gpu::Vec4f* attrs, gpu::Vertex& out) {
    out.pos = attrs[0];
    out.varying[0] = attrs[1];
    out.varying_count = 1;
}

// Toy FS: pass varying[0] (interpolated colour) straight out.
void fs_passthrough(const gpu::DrawState&, const gpu::Vec4f* varying, gpu::Vec4f& out) {
    out = varying[0];
}

void write_ppm(const char* path, const gpu::Framebuffer& fb) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << fb.width << " " << fb.height << "\n255\n";
    for (int y = fb.height - 1; y >= 0; --y) {
        for (int x = 0; x < fb.width; ++x) {
            const uint32_t c = fb.color[y * fb.width + x];
            const uint8_t r = (c >>  0) & 0xFF;
            const uint8_t g = (c >>  8) & 0xFF;
            const uint8_t b = (c >> 16) & 0xFF;
            f.put(static_cast<char>(r));
            f.put(static_cast<char>(g));
            f.put(static_cast<char>(b));
        }
    }
}

}  // namespace

int main() {
    constexpr int W = 64;
    constexpr int H = 64;

    // Triangle: positions in clip space (xyzw), colours rgba.
    static const std::array<gpu::Vec4f, 3> positions = {
        gpu::Vec4f{{ 0.0f,  0.8f, 0.0f, 1.0f}},
        gpu::Vec4f{{-0.8f, -0.8f, 0.0f, 1.0f}},
        gpu::Vec4f{{ 0.8f, -0.8f, 0.0f, 1.0f}},
    };
    static const std::array<gpu::Vec4f, 3> colours = {
        gpu::Vec4f{{1.0f, 0.0f, 0.0f, 1.0f}},
        gpu::Vec4f{{0.0f, 1.0f, 0.0f, 1.0f}},
        gpu::Vec4f{{0.0f, 0.0f, 1.0f, 1.0f}},
    };

    gpu::Context ctx{};
    ctx.fb.width  = W;
    ctx.fb.height = H;
    ctx.fb.color.assign(W * H, 0xFF202020u);  // dark grey clear
    ctx.draw.vp_x = 0; ctx.draw.vp_y = 0;
    ctx.draw.vp_w = W; ctx.draw.vp_h = H;
    ctx.draw.primitive = gpu::DrawState::TRIANGLES;

    ctx.attribs[0] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, positions.data()};
    ctx.attribs[1] = {true, 4, gpu::VertexAttribBinding::F32,
                      sizeof(gpu::Vec4f), 0, colours.data()};

    ctx.shaders.vs = vs_passthrough;
    ctx.shaders.fs = fs_passthrough;

    gpu::pipeline::draw(ctx, 3);

    // Sanity: count non-background pixels (expect roughly half the screen).
    int painted = 0;
    for (uint32_t c : ctx.fb.color) {
        if (c != 0xFF202020u) ++painted;
    }
    std::printf("painted %d / %d pixels\n", painted, W * H);

    if (const char* out = std::getenv("SMOKE_OUT")) {
        write_ppm(out, ctx.fb);
        std::printf("wrote %s\n", out);
    }

    // Skeleton acceptance: any non-zero coverage means pipeline plumbing OK.
    return painted > 0 ? 0 : 1;
}
