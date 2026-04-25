#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "gpu/texture.h"
#include "gpu/types.h"
#include "gpu_compiler/sim.h"

namespace gpu {
struct Context;     // forward decl from gpu/state.h
}

namespace gpu::systemc {

// TLM extension carrying a fragment-/vertex-shader execution request.
// Sprint 5 keeps it deliberately tiny: one ISA program + per-thread inputs.
// Phase-1.x will replace this with proper TLM-2.0 generic_payload extensions
// and per-block typed transactions.
struct ShaderJob {
    const std::vector<uint64_t>* code = nullptr;     // ISA binary (shared)
    std::array<gpu::sim::Vec4, 16> constants{};
    std::array<gpu::sim::Vec4, 8>  attrs{};          // VS only
    std::array<gpu::sim::Vec4, 8>  varying_in{};     // FS only
    int                            attr_count = 0;
    int                            varying_in_count = 0;
    bool                           is_vs = true;

    // outputs, populated by SC
    std::array<gpu::sim::Vec4, 4>  outputs{};
    bool                           lane_active = true;
};

// VertexFetchJob — driven by CP, fans out per-vertex ShaderJobs through SC.
struct VertexFetchJob {
    const std::vector<uint64_t>* vs_code = nullptr;
    std::array<gpu::sim::Vec4, 16> constants{};
    int attr_count = 0;
    int vertex_count = 0;
    std::vector<std::array<gpu::sim::Vec4, 8>> vertices;        // attrs per vertex
    std::vector<std::array<gpu::sim::Vec4, 4>> vs_outputs;      // populated by VF after SC
};

// PrimAssemblyJob — clip-space VS outputs in, screen-space triangles out.
struct PrimAssemblyJob {
    std::vector<std::array<gpu::sim::Vec4, 4>> vs_outputs;
    int  vp_x = 0, vp_y = 0, vp_w = 0, vp_h = 0;
    bool cull_back = false;
    std::vector<std::array<std::array<gpu::sim::Vec4, 4>, 3>> triangles;
};

// Sprint 14 RasterJob — screen-space triangles in, per-pixel fragments out.
struct RasterFragment {
    int     x, y;
    uint8_t coverage_mask;     // 4-bit when msaa_4x, else 1-bit
    float   depth;
    std::array<gpu::sim::Vec4, 4> varying;     // interpolated VS o1..o4
};
struct RasterJob {
    std::vector<std::array<std::array<gpu::sim::Vec4, 4>, 3>> triangles;
    int  fb_w = 0, fb_h = 0;
    bool msaa_4x = false;
    int  varying_count = 0;
    std::vector<RasterFragment> fragments;     // populated by RS
};

// Sprint 25 PfoJob — bundles a Quad + the Context the PFO state lives
// on. The CA block invokes gpu::pipeline::per_fragment_ops on it.
struct PfoJob {
    gpu::Context*     ctx  = nullptr;
    const gpu::Quad*  quad = nullptr;
};

// Sprint 26 TileFlushJob — passes a tile-resident Context* through
// TBF_ca (storage placeholder) and RSV_ca (gpu::pipeline::resolve
// wrapper). Tile bbox is informational; the resolve operates on the
// whole framebuffer for now.
struct TileFlushJob {
    gpu::Context* ctx = nullptr;
    int tile_x = 0, tile_y = 0;
    int tile_w = 16, tile_h = 16;
};

// Sprint 24 TextureJob — batch of (u,v) sample requests against one
// bound texture; results land in `samples` in order.
struct TextureSample { float u = 0.0f, v = 0.0f; };
struct TextureJob {
    const gpu::Texture*           tex = nullptr;
    std::vector<TextureSample>    requests;
    std::vector<std::array<float, 4>> samples;   // filled by TMU
};

}  // namespace gpu::systemc
