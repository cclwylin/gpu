// Per-lane `break` reconvergence (ISA v1.1).
//
// Each lane gets a different break threshold via r0.x:
//   lanes 0..3   -> threshold 3   (break after 3 iterations)
//   lanes 4..7   -> threshold 5
//   lanes 8..15  -> threshold 999 (never breaks during 10 iters)
//
// Loop body increments r1.x; break if (r1.x >= r0.x).
// Final r1.x recorded in o0.x. Asserts correct per-lane iteration counts —
// would fail under the Sprint-6 "warp-uniform break" approximation because
// the first lane to break would terminate the whole warp at iter 3.

#include <cmath>
#include <cstdio>

#include "gpu_compiler/asm.h"
#include "gpu_compiler/sim.h"

int main() {
    const std::string src =
        "loop 10\n"
        "  add r1.x, r1.xxxx, c0.xxxx\n"   // r1.x += 1.0
        "  setp_ge p, r1.x, r0.xxxx\n"     // p = (count >= threshold)
        "  (p) break\n"
        "endloop\n"
        "mov o0, r1\n";

    auto a = gpu::asm_::assemble(src);
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err line %d: %s\n", a.error_line, a.error.c_str());
        return 1;
    }

    gpu::sim::WarpState w{};
    for (int lane = 0; lane < gpu::sim::kWarpSize; ++lane) {
        float threshold;
        if      (lane < 4)  threshold = 3.0f;
        else if (lane < 8)  threshold = 5.0f;
        else                threshold = 999.0f;
        w.lane[lane].r[0] = {{threshold, 0, 0, 0}};
        w.lane[lane].r[1] = {{0, 0, 0, 0}};
        w.lane[lane].c[0] = {{1.0f, 0, 0, 0}};
    }

    auto er = gpu::sim::execute_warp(a.code, w);
    if (!er.ok) { std::fprintf(stderr, "exec err: %s\n", er.error.c_str()); return 1; }

    int fails = 0;
    auto check = [&](int lane, float expected) {
        float got = w.lane[lane].o[0][0];
        if (std::fabs(got - expected) > 1e-5f) {
            std::fprintf(stderr, "FAIL lane %d: got %g, want %g\n",
                         lane, (double)got, (double)expected);
            ++fails;
        }
    };
    for (int i = 0; i < 4;  ++i) check(i, 3.0f);   // broke at iter 3
    for (int i = 4; i < 8;  ++i) check(i, 5.0f);   // broke at iter 5
    for (int i = 8; i < 16; ++i) check(i, 10.0f);  // ran full 10 iters

    if (fails) { std::fprintf(stderr, "FAIL: %d\n", fails); return 1; }
    std::printf("PASS — divergent break across 16 lanes\n");
    return 0;
}
