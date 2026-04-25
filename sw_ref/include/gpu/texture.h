#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace gpu {

// TMU model — minimal Sprint-4 subset.
// Format support: RGBA8 only (RGB565 / ETC1 are Phase 1 follow-ups).
// Filter: NEAREST, BILINEAR. Wrap: CLAMP_TO_EDGE, REPEAT.
struct Texture {
    enum Format : uint8_t { RGBA8 } format = RGBA8;
    enum Filter : uint8_t { NEAREST, BILINEAR } filter = NEAREST;
    enum Wrap   : uint8_t { CLAMP, REPEAT } wrap_s = CLAMP, wrap_t = CLAMP;
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint32_t> texels;     // packed RGBA8, row-major, top-down
};

// Returns RGBA float in [0,1].
std::array<float, 4> sample_texture(const Texture& tex, float u, float v);

}  // namespace gpu
