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
    // For the skeleton, both shaders are C++ functors set up by the caller.
    // A real driver will instead hold compiled shader binaries.
    using VsFn = void (*)(const DrawState&, const Vec4f* attrs, Vertex& out_vert);
    using FsFn = void (*)(const DrawState&, const Vec4f* varying, Vec4f& out_color);
    VsFn vs = nullptr;
    FsFn fs = nullptr;
};

struct Context {
    DrawState draw;
    BoundShaderPair shaders;
    std::array<VertexAttribBinding, 8> attribs{};
    Framebuffer fb;
};

}  // namespace gpu
