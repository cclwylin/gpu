// 16-thread warp executor test. We run a divergent program where lanes
// 0..7 take one branch and lanes 8..15 take the other, and verify that the
// outputs differ accordingly.

#include <cmath>
#include <cstdio>

#include "gpu_compiler/asm.h"
#include "gpu_compiler/sim.h"

namespace {
int fails = 0;
#define EXPECT(cond)                                                      \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++fails;                                                      \
        }                                                                 \
    } while (0)
}

int main() {
    // Program:
    //   setp_lt p, r0.x, c0.x        ; per-lane: p = (lane_id < 8)
    //   if_p
    //       mov o0, c1               ; "red"  for lanes 0..7
    //   else
    //       mov o0, c2               ; "blue" for lanes 8..15
    //   endif
    const std::string src =
        "setp_lt p, r0.x, c0.xxxx\n"
        "if_p\n"
        "  mov o0, c1\n"
        "else\n"
        "  mov o0, c2\n"
        "endif\n";

    auto a = gpu::asm_::assemble(src);
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err line %d: %s\n",
                     a.error_line, a.error.c_str());
        return 1;
    }

    gpu::sim::WarpState w{};
    // Each lane gets its lane id in r0.x.
    for (int lane = 0; lane < gpu::sim::kWarpSize; ++lane) {
        w.lane[lane].r[0] = {{static_cast<float>(lane), 0, 0, 0}};
        // c0.x = 8.0 (threshold)
        w.lane[lane].c[0] = {{8.0f, 0, 0, 0}};
        // c1 = (1, 0, 0, 1) red, c2 = (0, 0, 1, 1) blue
        w.lane[lane].c[1] = {{1, 0, 0, 1}};
        w.lane[lane].c[2] = {{0, 0, 1, 1}};
    }

    auto er = gpu::sim::execute_warp(a.code, w);
    if (!er.ok) { std::fprintf(stderr, "exec err: %s\n", er.error.c_str()); return 1; }

    // Verify: lanes 0..7 -> red, lanes 8..15 -> blue.
    for (int lane = 0; lane < gpu::sim::kWarpSize; ++lane) {
        const auto& o = w.lane[lane].o[0];
        if (lane < 8) {
            EXPECT(std::fabs(o[0] - 1.0f) < 1e-5f);   // red
            EXPECT(std::fabs(o[2] - 0.0f) < 1e-5f);
        } else {
            EXPECT(std::fabs(o[0] - 0.0f) < 1e-5f);
            EXPECT(std::fabs(o[2] - 1.0f) < 1e-5f);   // blue
        }
    }

    if (fails) { std::fprintf(stderr, "FAIL: %d\n", fails); return 1; }
    std::printf("PASS — divergent if/else across 16 lanes\n");
    return 0;
}
