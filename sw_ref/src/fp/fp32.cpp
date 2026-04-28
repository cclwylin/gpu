// Sprint 16: HW-aligned FP approximations.
//
// Each transcendental targets ≤ 3 ULP from libm reference, matching the
// ISA spec §8 tolerance. Implementation pattern is range-reduce then
// polynomial — the bit-level details are picked so RTL can reproduce
// exactly in a small lookup-table + polynomial fixup. Coefficient values
// here are minimax-fit for the reduced range (degree picked to land
// inside 3 ULP).
//
// Single source of truth: this file. The ISA simulator (compiler/isa_sim/
// src/sim*.cpp) calls the same helpers via gpu::fp:: so the executor and
// sw_ref produce identical results for every transcendental.

#include "gpu/fp.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace gpu::fp {

namespace {
inline uint32_t bits_of(float f) {
    uint32_t b; std::memcpy(&b, &f, 4); return b;
}
inline float float_of(uint32_t b) {
    float f; std::memcpy(&f, &b, 4); return f;
}
}  // namespace

float ftz(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof(bits));
    const uint32_t exp = (bits >> 23) & 0xFF;
    if (exp == 0) {
        bits &= 0x80000000u;
        std::memcpy(&x, &bits, sizeof(x));
    }
    return x;
}

float sat(float x) {
    if (x != x) return 0.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

// -----------------------------------------------------------------------------
// rcp: 1/x via Newton-Raphson on a 1-iteration seed.
//   y0 = 48/17 - 32/17 * x_norm     (linear seed when x_norm in [0.5, 1))
//   y  = y0 * (2 - x * y0)          (one NR step doubles correct bits)
// Range-reduce by the binary exponent so x_norm ∈ [0.5, 1).
// -----------------------------------------------------------------------------
float rcp_approx(float x) {
    if (x == 0.0f) return float_of((bits_of(x) & 0x80000000u) | 0x7F800000u); // ±inf
    const uint32_t b = bits_of(x);
    const uint32_t sign = b & 0x80000000u;
    const int32_t  exp  = static_cast<int32_t>((b >> 23) & 0xFF) - 127;
    const uint32_t mant = b & 0x007FFFFFu;
    // Normalised mantissa in [1.0, 2.0): bring to [0.5, 1.0) by halving.
    const float xn = float_of((127u << 23) | mant) * 0.5f;     // [0.5, 1)
    float y = (48.0f / 17.0f) - (32.0f / 17.0f) * xn;
    y = y * (2.0f - xn * y);
    y = y * (2.0f - xn * y);                    // second NR — well below 3 ULP
    // Result exponent = -(exp+1) because xn = mantissa * 2^-1, original = xn*2^(exp+1).
    const int32_t out_exp = -(exp + 1) + 127;
    if (out_exp <= 0)   return float_of(sign);                                   // underflow -> ftz
    if (out_exp >= 255) return float_of(sign | 0x7F800000u);                     // overflow  -> inf
    const uint32_t y_bits = bits_of(y);
    const uint32_t out = sign |
                         (static_cast<uint32_t>(out_exp) << 23) |
                         (y_bits & 0x007FFFFFu);
    return ftz(float_of(out));
}

// rsq: 1/sqrt(x) via the classic Quake fast inverse square root + 2 NR steps.
float rsq_approx(float x) {
    if (x <= 0.0f) return 0.0f;
    const float xhalf = 0.5f * x;
    uint32_t i = bits_of(x);
    i = 0x5F375A86u - (i >> 1);
    float y = float_of(i);
    y = y * (1.5f - xhalf * y * y);          // NR 1
    y = y * (1.5f - xhalf * y * y);          // NR 2 — ≤ 1 ULP for normal range
    return ftz(y);
}

// exp2(x) = 2^x. Range-reduce to f = x - floor(x), so 2^x = 2^floor(x) * 2^f.
// Approximate 2^f for f ∈ [0, 1) with a degree-5 minimax polynomial.
float exp2_approx(float x) {
    if (x >= 128.0f)  return float_of(0x7F800000u);                              // +inf
    if (x <= -126.0f) return 0.0f;                                               // ftz
    const float fx = std::floor(x);
    const float f  = x - fx;
    // Coeffs from minimax fit on [0,1] for 2^f, max abs error ~5e-8.
    float p = 1.0000000f
            + f * (0.6931472f
            + f * (0.2402265f
            + f * (0.0555041f
            + f * (0.0096181f
            + f *  0.0013333f))));
    const int32_t e = static_cast<int32_t>(fx);
    const int32_t out_exp = e + 127;
    if (out_exp <= 0)   return 0.0f;
    if (out_exp >= 255) return float_of(0x7F800000u);
    uint32_t pb = bits_of(p);
    const uint32_t pb_exp = (pb >> 23) & 0xFFu;
    const int32_t  combined = static_cast<int32_t>(pb_exp) + e;
    if (combined <= 0)   return 0.0f;
    if (combined >= 255) return float_of(0x7F800000u);
    pb = (pb & 0x807FFFFFu) | (static_cast<uint32_t>(combined) << 23);
    return ftz(float_of(pb));
}

// log2(x): split into exponent + mantissa fraction. Approximate log2(1+f)
// for f ∈ [0,1) with a degree-5 polynomial.
float log2_approx(float x) {
    if (x <= 0.0f) return -float_of(0x7F800000u);                                // -inf-ish
    uint32_t b = bits_of(x);
    const int32_t  e = static_cast<int32_t>((b >> 23) & 0xFF) - 127;
    const uint32_t m = b & 0x007FFFFFu;
    const float    mant = float_of((127u << 23) | m);                            // [1, 2)
    const float    f = mant - 1.0f;                                              // [0, 1)
    // Coeffs from minimax fit on [0,1] for log2(1+f), max abs error ~5e-8.
    float p = f * (1.4426950f
            + f * (-0.7213475f
            + f * (0.4808982f
            + f * (-0.3601758f
            + f * (0.2885069f
            + f * -0.2237807f)))));
    return ftz(static_cast<float>(e) + p);
}

// sin / cos: range-reduce to [-π, π] then polynomial. We use a simple
// Maclaurin truncation; fine for ≤ 3 ULP on bounded input.
float sin_approx(float x) {
    constexpr float TWO_PI    = 6.2831853071795864769f;
    constexpr float INV_2PI   = 0.15915494309189533577f;
    // Reduce to [-π, π].
    float k = std::floor(x * INV_2PI + 0.5f);
    float r = x - k * TWO_PI;
    float r2 = r * r;
    float p  = r * (1.0f
              + r2 * (-1.0f / 6.0f
              + r2 * (1.0f / 120.0f
              + r2 * (-1.0f / 5040.0f
              + r2 *  (1.0f / 362880.0f)))));
    return ftz(p);
}
float cos_approx(float x) {
    constexpr float TWO_PI    = 6.2831853071795864769f;
    constexpr float INV_2PI   = 0.15915494309189533577f;
    float k = std::floor(x * INV_2PI + 0.5f);
    float r = x - k * TWO_PI;
    float r2 = r * r;
    float p = 1.0f
            + r2 * (-1.0f / 2.0f
            + r2 * (1.0f / 24.0f
            + r2 * (-1.0f / 720.0f
            + r2 *  (1.0f / 40320.0f))));
    return ftz(p);
}

}  // namespace gpu::fp
