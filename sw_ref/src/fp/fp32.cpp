#include "gpu/fp.h"

#include <cmath>
#include <cstring>

namespace gpu::fp {

float ftz(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof(bits));
    const uint32_t exp = (bits >> 23) & 0xFF;
    if (exp == 0) {
        // Subnormal or zero -> zero, preserve sign.
        bits &= 0x80000000u;
        std::memcpy(&x, &bits, sizeof(x));
    }
    return x;
}

float sat(float x) {
    if (x != x) return 0.0f;     // NaN -> 0 per OpenGL convention
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

// Skeleton implementations: defer to libm. Phase 1 will replace with
// bit-aligned approximations matching HW (3-ULP tolerance per ISA spec §7).
float rcp_approx(float x)   { return ftz(1.0f / x); }
float rsq_approx(float x)   { return ftz(1.0f / std::sqrt(x)); }
float exp2_approx(float x)  { return ftz(std::exp2(x)); }
float log2_approx(float x)  { return ftz(std::log2(x)); }
float sin_approx(float x)   { return ftz(std::sin(x)); }
float cos_approx(float x)   { return ftz(std::cos(x)); }

}  // namespace gpu::fp
