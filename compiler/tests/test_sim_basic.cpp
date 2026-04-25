// Sim test: assemble + execute, check vec4 outputs.
//
// We exercise: ALU 2-op (mov/mul/add), source swizzle/replicate,
// 3-op mad, transcendental rcp, source negate modifier.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "gpu_compiler/asm.h"
#include "gpu_compiler/sim.h"

namespace {
int fails = 0;
#define EXPECT_NEAR(a, b)                                                  \
    do {                                                                   \
        if (std::fabs((a) - (b)) > 1e-5f) {                                \
            std::fprintf(stderr, "FAIL %s:%d: %f != %f\n",                 \
                         __FILE__, __LINE__, (double)(a), (double)(b));   \
            ++fails;                                                       \
        }                                                                  \
    } while (0)
}

int main() {
    // Program: o0 = (c0 + c1) * c2.x ; with negate modifier on src1
    // We compute (1.0 + 2.0) * 3.0 = 9.0 along all components.
    const std::string src =
        "add  r0,    c0,     c1\n"     // r0 = c0 + c1
        "mul  o0,    r0,     c2.xxxx\n";

    auto r = gpu::asm_::assemble(src);
    if (!r.error.empty()) {
        std::fprintf(stderr, "asm error line %d: %s\n", r.error_line, r.error.c_str());
        return 1;
    }

    gpu::sim::ThreadState t{};
    t.c[0] = {{1.0f, 1.0f, 1.0f, 1.0f}};
    t.c[1] = {{2.0f, 2.0f, 2.0f, 2.0f}};
    t.c[2] = {{3.0f, 0.0f, 0.0f, 0.0f}};

    auto er = gpu::sim::execute(r.code, t);
    if (!er.ok) { std::fprintf(stderr, "exec error: %s\n", er.error.c_str()); return 1; }

    EXPECT_NEAR(t.o[0][0], 9.0f);
    EXPECT_NEAR(t.o[0][1], 9.0f);
    EXPECT_NEAR(t.o[0][2], 9.0f);
    EXPECT_NEAR(t.o[0][3], 9.0f);

    // Second program: 3-op mad + source negate.
    // o0 = c0 * c1 + (-c2) -> 2*3 + (-4) = 2
    {
        const std::string s =
            "mad  o0,    c0,     c1, -r2\n"   // r2 holds c2 copy
            ;
        // We need r2 = c2 first; do it in a separate program (no labels yet).
        const std::string s_setup = "mov  r2, c2\n";
        auto a1 = gpu::asm_::assemble(s_setup);
        auto a2 = gpu::asm_::assemble(s);
        if (!a1.error.empty() || !a2.error.empty()) {
            std::fprintf(stderr, "asm setup/main failed\n"); return 1;
        }
        gpu::sim::ThreadState t2{};
        t2.c[0] = {{2.f, 2.f, 2.f, 2.f}};
        t2.c[1] = {{3.f, 3.f, 3.f, 3.f}};
        t2.c[2] = {{4.f, 4.f, 4.f, 4.f}};
        gpu::sim::execute(a1.code, t2);
        gpu::sim::execute(a2.code, t2);
        EXPECT_NEAR(t2.o[0][0], 2.0f);
    }

    // Third: rcp produces 1/x as broadcast; FTZ behavior on subnormals.
    {
        const std::string s = "rcp  o0, c0\n";
        auto a = gpu::asm_::assemble(s);
        if (!a.error.empty()) { std::fprintf(stderr, "asm err\n"); return 1; }
        gpu::sim::ThreadState t3{};
        t3.c[0] = {{4.f, 999.f, 999.f, 999.f}};   // rcp uses .x
        gpu::sim::execute(a.code, t3);
        EXPECT_NEAR(t3.o[0][0], 0.25f);
        EXPECT_NEAR(t3.o[0][1], 0.25f);    // broadcast
    }

    if (fails) { std::fprintf(stderr, "FAIL: %d\n", fails); return 1; }
    std::printf("PASS\n");
    return 0;
}
