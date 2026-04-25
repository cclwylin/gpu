// GLSL frontend integration test:
//   1. Compile a small inline GLSL VS to ISA via gpu_compiler::glsl::compile.
//   2. Run it through compiler/isa_sim/ on synthetic inputs.
//   3. Compare against the same computation done by hand in C++.
//
// This proves the lexer/parser/codegen path produces a working program for
// the patterns used by ref_shader_1 (mat4 * vec4 + vec4 multiply + pass).

#include <cmath>
#include <cstdio>

#include "gpu_compiler/glsl.h"
#include "gpu_compiler/sim.h"

namespace {
int fails = 0;
#define EXPECT_NEAR(a, b)                                                  \
    do {                                                                   \
        if (std::fabs((a) - (b)) > 1e-5f) {                                \
            std::fprintf(stderr, "FAIL %s:%d: %g != %g\n",                 \
                         __FILE__, __LINE__, (double)(a), (double)(b));   \
            ++fails;                                                       \
        }                                                                  \
    } while (0)
}

int main() {
    const char* glsl =
        "attribute vec4 a_pos;\n"
        "attribute vec4 a_color;\n"
        "uniform mat4 u_mvp;\n"
        "uniform vec4 u_tint;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * a_pos;\n"
        "    v_color = a_color * u_tint;\n"
        "}\n";

    auto r = gpu::glsl::compile(glsl, gpu::glsl::ShaderStage::Vertex);
    if (!r.error.empty()) {
        std::fprintf(stderr, "compile error at line %d: %s\n",
                     r.error_line, r.error.c_str());
        return 1;
    }

    // Sanity on ABI metadata
    if (r.attributes.size() != 2)            { std::fprintf(stderr, "attr count wrong\n"); ++fails; }
    if (r.uniforms.size() != 2)              { std::fprintf(stderr, "uniform count wrong\n"); ++fails; }
    if (r.varyings_out.size() != 1)          { std::fprintf(stderr, "varying out count wrong\n"); ++fails; }
    if (r.uniforms[0].slot != 0)             { std::fprintf(stderr, "u_mvp slot wrong\n"); ++fails; }
    if (r.uniforms[1].slot != 4)             { std::fprintf(stderr, "u_tint slot wrong\n"); ++fails; }

    // Set up sim state.
    gpu::sim::ThreadState t{};
    // a_pos = (1, 2, 3, 1)
    t.r[0] = {{1.0f, 2.0f, 3.0f, 1.0f}};
    // a_color = (0.5, 0.6, 0.7, 1.0)
    t.r[1] = {{0.5f, 0.6f, 0.7f, 1.0f}};

    // u_mvp: identity matrix in c0..c3 (row-major).
    // c0 = row0 = (1,0,0,0)  c1 = (0,1,0,0)  c2 = (0,0,1,0)  c3 = (0,0,0,1)
    t.c[0] = {{1, 0, 0, 0}};
    t.c[1] = {{0, 1, 0, 0}};
    t.c[2] = {{0, 0, 1, 0}};
    t.c[3] = {{0, 0, 0, 1}};
    // u_tint = (1, 0.5, 1, 1)
    t.c[4] = {{1.0f, 0.5f, 1.0f, 1.0f}};

    auto er = gpu::sim::execute(r.code, t);
    if (!er.ok) { std::fprintf(stderr, "sim error: %s\n", er.error.c_str()); return 1; }

    // o0 = u_mvp * a_pos == a_pos when u_mvp is identity.
    EXPECT_NEAR(t.o[0][0], 1.0f);
    EXPECT_NEAR(t.o[0][1], 2.0f);
    EXPECT_NEAR(t.o[0][2], 3.0f);
    EXPECT_NEAR(t.o[0][3], 1.0f);

    // o1 = a_color * u_tint
    EXPECT_NEAR(t.o[1][0], 0.5f * 1.0f);
    EXPECT_NEAR(t.o[1][1], 0.6f * 0.5f);
    EXPECT_NEAR(t.o[1][2], 0.7f * 1.0f);
    EXPECT_NEAR(t.o[1][3], 1.0f * 1.0f);

    if (fails) { std::fprintf(stderr, "FAIL: %d\n", fails); return 1; }
    std::printf("PASS (%zu inst)\n", r.code.size());
    return 0;
}
