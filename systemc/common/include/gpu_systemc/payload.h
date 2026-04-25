#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "gpu_compiler/sim.h"

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

}  // namespace gpu::systemc
