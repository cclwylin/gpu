#pragma once
#include <array>
#include <cstdint>

#include "types.h"

namespace gpu {

// ---------------------------------------------------------------------------
// GL state — minimal v0 used by the skeleton. Will grow per Phase 1.
// ---------------------------------------------------------------------------
struct VertexAttribBinding {
    bool enabled = false;
    uint8_t component_count = 4;     // 1..4
    enum Format : uint8_t { F32, U8N, U16N } format = F32;
    uint32_t stride = 0;
    uint32_t offset = 0;
    const void* data = nullptr;       // host pointer for skeleton (real driver: VBO)
};

struct DrawState {
    enum PrimitiveMode : uint8_t {
        TRIANGLES,
        TRIANGLE_STRIP,
        TRIANGLE_FAN,
        // POINTS / LINES: out of skeleton scope
    } primitive = TRIANGLES;

    bool depth_test  = false;
    bool depth_write = true;
    bool cull_back   = false;
    bool a2c         = false;

    // Viewport
    int32_t vp_x = 0;
    int32_t vp_y = 0;
    int32_t vp_w = 0;
    int32_t vp_h = 0;

    // Uniform constant bank (c0..c15) used by both VS and FS in this skeleton.
    std::array<Vec4f, 16> uniforms{};
};

struct BoundShaderPair {
    // Sprint-1 model: each side is *either* a C++ functor (legacy) *or* a
    // compiled ISA binary that we run through compiler/isa_sim/. Sprint 1
    // exercises both paths so we can A/B verify.
    using VsFn = void (*)(const DrawState&, const Vec4f* attrs, Vertex& out_vert);
    using FsFn = void (*)(const DrawState&, const Vec4f* varying, Vec4f& out_color);
    VsFn vs = nullptr;
    FsFn fs = nullptr;

    // Optional ISA binaries. If set, take precedence over the functor.
    // Held as opaque pointers so sw_ref/state.h doesn't depend on compiler/.
    const void* vs_binary = nullptr;     // -> std::vector<uint64_t>
    const void* fs_binary = nullptr;
    int  vs_attr_count = 0;              // attribute slots used as r0..r{N-1}
    int  vs_varying_count = 0;
    int  fs_varying_count = 0;
};

struct Context {
    DrawState draw;
    BoundShaderPair shaders;
    std::array<VertexAttribBinding, 8> attribs{};
    Framebuffer fb;
};

}  // namespace gpu
