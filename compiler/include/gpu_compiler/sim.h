#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "encoding.h"

namespace gpu::sim {

// Sprint-1 ISA simulator: single-thread executor (one "lane").
// Phase 1 follow-up will widen to 16-thread warp + per-lane mask.

struct Vec4 {
    std::array<float, 4> v{};
    float& operator[](int i) { return v[i]; }
    float  operator[](int i) const { return v[i]; }
};

// Texture sampling callback: caller provides the actual TMU model.
//   slot — texture binding (0..15)
//   uv   — sample coordinate (uv.x, uv.y); other lanes available for future modes
//   mode — 0 plain, 1 bias, 2 lod, 3 grad
//   bias_or_lod — when mode != 0, bias / lod / grad-strength scalar
// Returns RGBA8 normalised to [0,1].
using TexSampler = std::function<Vec4(uint8_t slot, Vec4 uv, uint8_t mode, float bias_or_lod)>;

struct ThreadState {
    std::array<Vec4, 32> r{};        // GPRs
    std::array<Vec4, 16> c{};        // constants (warp-shared in real HW)
    std::array<Vec4, 8>  varying{};  // FS inputs (per-thread)
    std::array<Vec4, 4>  o{};        // outputs
    bool                 lane_active = true;       // killed via `kil`
    bool                 predicate = false;        // single-lane analog of warp p
    int                  varying_count = 0;
};

struct ExecResult {
    bool ok = true;
    std::string error;
};

// Run a shader binary on a thread state until program end.
// `tex` may be null if the shader doesn't sample textures.
ExecResult execute(const std::vector<isa::Inst>& code,
                   ThreadState& t,
                   TexSampler tex = nullptr);

}  // namespace gpu::sim
