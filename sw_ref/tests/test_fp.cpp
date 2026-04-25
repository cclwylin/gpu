// Spot-checks for FP helpers — not the full HW-aligned suite (Phase 1).
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
}  // namespace

int main() {
    // Saturate
    EXPECT(gpu::fp::sat(-0.5f) == 0.0f);
    EXPECT(gpu::fp::sat( 1.5f) == 1.0f);
    EXPECT(gpu::fp::sat( 0.5f) == 0.5f);
    EXPECT(gpu::fp::sat(std::nanf("")) == 0.0f);  // NaN -> 0

    // Flush-to-zero on subnormals
    const float subnormal_pos = bit_cast_to_float(0x00400000u); // smallest subnormal
    EXPECT(gpu::fp::ftz(subnormal_pos) == 0.0f);
    const float negzero = bit_cast_to_float(0x80000000u);
    EXPECT(gpu::fp::ftz(negzero) == negzero);     // signed zero preserved
    EXPECT(gpu::fp::ftz(1.0f) == 1.0f);

    // Approximations smoke
    EXPECT(std::fabs(gpu::fp::rcp_approx(2.0f) - 0.5f) < 1e-6f);
    EXPECT(std::fabs(gpu::fp::rsq_approx(4.0f) - 0.5f) < 1e-6f);
    EXPECT(std::fabs(gpu::fp::exp2_approx(3.0f) - 8.0f) < 1e-5f);
    EXPECT(std::fabs(gpu::fp::log2_approx(8.0f) - 3.0f) < 1e-5f);

    if (fails) std::fprintf(stderr, "FAIL: %d assertion(s)\n", fails);
    else        std::printf("PASS\n");
    return fails;
}
