#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace gpu {

// ---------------------------------------------------------------------------
// Math primitives — small, header-only, deliberate. We do NOT use eigen/glm to
// keep the FP path under our control and bit-aligned with HW (see fp/fp32.h).
// ---------------------------------------------------------------------------

template <typename T, std::size_t N>
struct Vec {
    std::array<T, N> v{};
    T& operator[](std::size_t i) { return v[i]; }
    const T& operator[](std::size_t i) const { return v[i]; }
    constexpr std::size_t size() const { return N; }
};

using Vec2f = Vec<float, 2>;
using Vec3f = Vec<float, 3>;
using Vec4f = Vec<float, 4>;
using Vec4u8 = Vec<uint8_t, 4>;

struct Mat4 {
    // Column-major (matches OpenGL convention).
    std::array<float, 16> m{};
    float& operator()(int r, int c) { return m[c * 4 + r]; }
    float operator()(int r, int c) const { return m[c * 4 + r]; }
};

// ---------------------------------------------------------------------------
// Pipeline payloads
// ---------------------------------------------------------------------------

struct Vertex {
    Vec4f pos;                  // post-VS clip-space
    std::array<Vec4f, 7> varying{};  // o1..o7 (FS v0..v6)
    uint8_t varying_count = 0;
};

struct Triangle {
    std::array<Vertex, 3> v{};
};

struct ScreenPos {
    int32_t x;        // sub-pixel? Phase 0: integer pixel for skeleton
    int32_t y;
};

struct Fragment {
    ScreenPos pos;
    uint8_t coverage_mask;       // 4-bit (4× MSAA) or 1-bit (1×) in low bits
    std::array<Vec4f, 7> varying{};
    float depth = 0.0f;
    uint8_t varying_count = 0;
    // Sprint 46 — set by the rasterizer from sign(area). PFO uses it to
    // pick front/back stencil state for VK-GL-CTS fragment_ops.depth_stencil.*
    // (which submits back-facing geometry expecting separate back stencil).
    bool front_facing = true;
};

struct Pixel {
    Vec4u8 color = {0, 0, 0, 0};   // RGBA8
};

// 2x2 quad (rasterizer outputs in quads for derivative support)
struct Quad {
    std::array<Fragment, 4> frags;
};

// ---------------------------------------------------------------------------
// Framebuffer descriptor
// ---------------------------------------------------------------------------
struct Framebuffer {
    int32_t width = 0;
    int32_t height = 0;
    bool msaa_4x = false;
    bool a2c     = false;
    // Backing storage (post-resolve, RGBA8 packed).
    std::vector<uint32_t> color;
    // 1× depth buffer (used when msaa_4x = false). Float depth in [0,1].
    std::vector<float>    depth;
    // 1× stencil buffer (Sprint 17). 8-bit unsigned per pixel.
    std::vector<uint8_t>  stencil;
    // Per-sample storage during render (msaa_4x = true).
    std::vector<uint32_t> color_samples;
    std::vector<float>    depth_samples;
    std::vector<uint8_t>  stencil_samples;
};

}  // namespace gpu
