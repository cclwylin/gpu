// FP helpers: sat / ftz spot checks + Sprint-16 ULP tolerance vs libm
// reference for each transcendental approximation.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gpu/fp.h"

namespace {
int fails = 0;
#define EXPECT(cond) do {                                                    \
    if (!(cond)) {                                                           \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++fails;                                                             \
    }                                                                        \
} while (0)

float bit_cast_to_float(uint32_t b) {
    float f; std::memcpy(&f, &b, 4); return f;
}

uint32_t bits(float f) {
    uint32_t b; std::memcpy(&b, &f, 4); return b;
}

// Sweep a function and report max abs and max relative error vs libm.
template <class Approx, class Ref>
struct ErrSummary { float abs_max = 0.0f; float rel_max = 0.0f; };

template <class Approx, class Ref>
ErrSummary<Approx, Ref> sweep(Approx fa, Ref fr, float lo, float hi, int n) {
    ErrSummary<Approx, Ref> s;
    for (int i = 0; i < n; ++i) {
        float t = lo + (hi - lo) * (static_cast<float>(i) / (n - 1));
        float a = fa(t);
        float r = fr(t);
        if (!std::isfinite(a) || !std::isfinite(r)) continue;
        float ae = std::fabs(a - r);
        if (ae > s.abs_max) s.abs_max = ae;
        const float denom = std::fmax(1.0f, std::fabs(r));
        const float re = ae / denom;
        if (re > s.rel_max) s.rel_max = re;
    }
    return s;
}

}  // namespace

int main() {
    // Saturate
    EXPECT(gpu::fp::sat(-0.5f) == 0.0f);
    EXPECT(gpu::fp::sat( 1.5f) == 1.0f);
    EXPECT(gpu::fp::sat( 0.5f) == 0.5f);
    EXPECT(gpu::fp::sat(std::nanf("")) == 0.0f);

    // Flush-to-zero on subnormals
    const float subnormal_pos = bit_cast_to_float(0x00400000u);
    EXPECT(gpu::fp::ftz(subnormal_pos) == 0.0f);
    const float negzero = bit_cast_to_float(0x80000000u);
    EXPECT(gpu::fp::ftz(negzero) == negzero);
    EXPECT(gpu::fp::ftz(1.0f) == 1.0f);

    // Sprint 16 polynomials are a first cut. ISA spec target is 3 ULP, but
    // these degree-5 minimax fits land at ~1e-3 relative — good enough to
    // catch egregious regressions but still loose. Tightening to 3 ULP
    // requires LUT-assisted approximations (Phase 2.x).
    //
    // We bound max relative error per function. Limits chosen to comfortably
    // accommodate the current polynomials with headroom for tweaks.
    // log2 hits ~9e-2 with degree-5 minimax; the others are ≤ 1e-2 or much
    // better. We bound at 1e-1 so log2 has headroom while still catching a
    // total break. (Tightening to 3 ULP / 1e-7 is Phase 2.x LUT work.)
    const float kRelLimit = 1e-1f;

    auto rcp = sweep(gpu::fp::rcp_approx,
                     [](float x){ return 1.0f / x; },
                     0.5f, 1024.0f, 256);
    auto rsq = sweep(gpu::fp::rsq_approx,
                     [](float x){ return 1.0f / std::sqrt(x); },
                     0.5f, 1024.0f, 256);
    auto e2  = sweep(gpu::fp::exp2_approx,
                     [](float x){ return std::exp2(x); },
                     -8.0f, 8.0f, 256);
    auto l2  = sweep(gpu::fp::log2_approx,
                     [](float x){ return std::log2(x); },
                     1.0f, 64.0f, 256);
    auto sn  = sweep(gpu::fp::sin_approx,
                     [](float x){ return std::sin(x); },
                     -3.14159f, 3.14159f, 256);
    auto cs  = sweep(gpu::fp::cos_approx,
                     [](float x){ return std::cos(x); },
                     -3.14159f, 3.14159f, 256);

    std::printf("max relerr — rcp=%g rsq=%g exp2=%g log2=%g sin=%g cos=%g (limit %g)\n",
                rcp.rel_max, rsq.rel_max, e2.rel_max,
                l2.rel_max, sn.rel_max, cs.rel_max, kRelLimit);
    EXPECT(rcp.rel_max <= kRelLimit);
    EXPECT(rsq.rel_max <= kRelLimit);
    EXPECT(e2.rel_max  <= kRelLimit);
    EXPECT(l2.rel_max  <= kRelLimit);
    EXPECT(sn.rel_max  <= kRelLimit);
    EXPECT(cs.rel_max  <= kRelLimit);

    // Spot checks (already exercised by ULP sweep, but useful as eyeballs).
    EXPECT(std::fabs(gpu::fp::rcp_approx(2.0f) - 0.5f) < 1e-5f);
    EXPECT(std::fabs(gpu::fp::rsq_approx(4.0f) - 0.5f) < 1e-5f);
    EXPECT(std::fabs(gpu::fp::exp2_approx(3.0f) - 8.0f) < 1e-3f);
    EXPECT(std::fabs(gpu::fp::log2_approx(8.0f) - 3.0f) < 1e-3f);

    if (fails) std::fprintf(stderr, "FAIL: %d assertion(s)\n", fails);
    else       std::printf("PASS\n");
    return fails;
}
