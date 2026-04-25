#include "gpu/texture.h"

#include <algorithm>
#include <cmath>

namespace gpu {

namespace {
inline float wrap_coord(float c, Texture::Wrap w) {
    switch (w) {
        case Texture::CLAMP:  return std::clamp(c, 0.0f, 1.0f);
        case Texture::REPEAT: return c - std::floor(c);
    }
    return c;
}

inline std::array<float, 4> unpack(uint32_t px) {
    return {
        static_cast<float>((px >>  0) & 0xFF) / 255.0f,
        static_cast<float>((px >>  8) & 0xFF) / 255.0f,
        static_cast<float>((px >> 16) & 0xFF) / 255.0f,
        static_cast<float>((px >> 24) & 0xFF) / 255.0f,
    };
}

inline std::array<float, 4> fetch(const Texture& t, int x, int y) {
    x = std::clamp(x, 0, t.width  - 1);
    y = std::clamp(y, 0, t.height - 1);
    return unpack(t.texels[static_cast<size_t>(y) * t.width + x]);
}

inline std::array<float, 4> lerp4(const std::array<float, 4>& a,
                                  const std::array<float, 4>& b, float t) {
    return {
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
        a[3] + (b[3] - a[3]) * t,
    };
}
}  // namespace

std::array<float, 4> sample_texture(const Texture& t, float u, float v) {
    if (t.width <= 0 || t.height <= 0 || t.texels.empty()) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    const float us = wrap_coord(u, t.wrap_s);
    const float vs = wrap_coord(v, t.wrap_t);
    const float fx = us * t.width  - 0.5f;
    const float fy = vs * t.height - 0.5f;

    if (t.filter == Texture::NEAREST) {
        const int ix = static_cast<int>(std::floor(fx + 0.5f));
        const int iy = static_cast<int>(std::floor(fy + 0.5f));
        return fetch(t, ix, iy);
    }
    // Bilinear
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float ax = fx - x0;
    const float ay = fy - y0;
    const auto t00 = fetch(t, x0,     y0);
    const auto t10 = fetch(t, x0 + 1, y0);
    const auto t01 = fetch(t, x0,     y0 + 1);
    const auto t11 = fetch(t, x0 + 1, y0 + 1);
    const auto top    = lerp4(t00, t10, ax);
    const auto bottom = lerp4(t01, t11, ax);
    return lerp4(top, bottom, ay);
}

}  // namespace gpu
