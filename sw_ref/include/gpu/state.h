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

    // Blend state. Sprint 46 — full ES 2.0 surface (separate RGB / alpha
    // factors and equations + constant blend color + GL_CONSTANT_* and
    // GL_SRC_ALPHA_SATURATE factors). VK-GL-CTS fragment_ops.blend.*.
    bool blend_enable = false;
    enum BlendFactor : uint8_t {
        BF_ZERO, BF_ONE,
        BF_SRC_COLOR,  BF_ONE_MINUS_SRC_COLOR,
        BF_DST_COLOR,  BF_ONE_MINUS_DST_COLOR,
        BF_SRC_ALPHA,  BF_ONE_MINUS_SRC_ALPHA,
        BF_DST_ALPHA,  BF_ONE_MINUS_DST_ALPHA,
        BF_CONSTANT_COLOR, BF_ONE_MINUS_CONSTANT_COLOR,
        BF_CONSTANT_ALPHA, BF_ONE_MINUS_CONSTANT_ALPHA,
        BF_SRC_ALPHA_SATURATE,
    };
    BlendFactor blend_src_rgb   = BF_SRC_ALPHA;
    BlendFactor blend_dst_rgb   = BF_ONE_MINUS_SRC_ALPHA;
    BlendFactor blend_src_alpha = BF_SRC_ALPHA;
    BlendFactor blend_dst_alpha = BF_ONE_MINUS_SRC_ALPHA;
    enum BlendEq : uint8_t {
        BE_ADD, BE_SUBTRACT, BE_REVERSE_SUBTRACT,
    };
    BlendEq blend_eq_rgb   = BE_ADD;
    BlendEq blend_eq_alpha = BE_ADD;
    Vec4f   blend_color    = {{0.0f, 0.0f, 0.0f, 0.0f}};

    // Stencil state. Sprint 46 — split into front + back faces (with the
    // unsuffixed members kept as the front state for backward compat).
    // PFO selects the right face via Fragment::front_facing.
    bool    stencil_test  = false;
    enum StencilFunc : uint8_t {
        SF_NEVER, SF_LESS, SF_LEQUAL, SF_GREATER, SF_GEQUAL,
        SF_EQUAL, SF_NOTEQUAL, SF_ALWAYS,
    };
    enum StencilOp : uint8_t {
        SO_KEEP, SO_ZERO, SO_REPLACE, SO_INCR, SO_DECR, SO_INVERT,
        SO_INCR_WRAP, SO_DECR_WRAP,
    };
    // Front-face state (also used as the unified state when glStencilFunc
    // / glStencilOp / glStencilMask is called instead of the *Separate forms).
    StencilFunc stencil_func = SF_ALWAYS;
    StencilOp   sop_fail  = SO_KEEP;
    StencilOp   sop_zfail = SO_KEEP;
    StencilOp   sop_zpass = SO_KEEP;
    uint8_t stencil_ref         = 0;
    uint8_t stencil_read_mask   = 0xFF;
    uint8_t stencil_write_mask  = 0xFF;
    // Back-face state — initialised to mirror the front so existing tests
    // that only set the unified state see identical behaviour.
    StencilFunc stencil_func_back = SF_ALWAYS;
    StencilOp   sop_fail_back  = SO_KEEP;
    StencilOp   sop_zfail_back = SO_KEEP;
    StencilOp   sop_zpass_back = SO_KEEP;
    uint8_t stencil_ref_back         = 0;
    uint8_t stencil_read_mask_back   = 0xFF;
    uint8_t stencil_write_mask_back  = 0xFF;

    // Scissor (Sprint 17). When enabled, fragments outside the box are
    // discarded by the rasterizer (treated as zero coverage).
    bool    scissor_enable = false;
    int32_t scissor_x = 0, scissor_y = 0, scissor_w = 0, scissor_h = 0;

    // Color buffer write mask (Sprint 43, RGBA). false = preserve the
    // existing channel of the destination. Applied by PFO on draws and by
    // glcompat::glClear so the masked CTS clear-tests reproduce.
    bool color_writemask[4] = {true, true, true, true};

    bool cull_back   = false;
    bool a2c         = false;

    // Viewport
    int32_t vp_x = 0;
    int32_t vp_y = 0;
    int32_t vp_w = 0;
    int32_t vp_h = 0;

    // Uniform constant bank (c0..c31) used by both VS and FS. Sprint 56 —
    // grew from 16 to 32 (the ISA s0idx field is 5 bits, so 32 slots are
    // already addressable). The bigger c-bank lets dEQP's random shaders
    // pack their VS+FS uniforms + literals without colliding.
    std::array<Vec4f, 32> uniforms{};
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
