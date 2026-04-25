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

}  // namespace gpu::systemc
