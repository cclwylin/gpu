#pragma once
#include <array>
#include <cstdint>

#include "texture.h"
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
    } primitive = TRIANGLES;

    // Depth state
    bool depth_test  = false;
    bool depth_write = true;
    enum DepthFunc : uint8_t {
        DF_LESS, DF_LEQUAL, DF_EQUAL, DF_GEQUAL,
        DF_GREATER, DF_NOTEQUAL, DF_ALWAYS, DF_NEVER,
    } depth_func = DF_LESS;

    // Blend state (ES 2.0 subset; no logic op, no separate alpha blend).
    bool blend_enable = false;
    enum BlendFactor : uint8_t {
        BF_ZERO, BF_ONE,
        BF_SRC_COLOR,  BF_ONE_MINUS_SRC_COLOR,
        BF_DST_COLOR,  BF_ONE_MINUS_DST_COLOR,
        BF_SRC_ALPHA,  BF_ONE_MINUS_SRC_ALPHA,
        BF_DST_ALPHA,  BF_ONE_MINUS_DST_ALPHA,
    } blend_src = BF_SRC_ALPHA, blend_dst = BF_ONE_MINUS_SRC_ALPHA;
    enum BlendEq : uint8_t {
        BE_ADD, BE_SUBTRACT, BE_REVERSE_SUBTRACT,
    } blend_eq = BE_ADD;

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
    // Bound textures by slot (matches ISA `tex N` binding index).
    std::array<const Texture*, 16> textures{};
};

}  // namespace gpu
