// SystemC TLM-LT smoke: assemble a tiny VS, send it through the CP→SC TLM
// wiring, and check that the SC ran the shader and produced expected outputs.

#include <cstdio>
#include <systemc>
#include <vector>

#include "gpu_compiler/asm.h"
#include "gpu_systemc/gpu_top.h"

int sc_main(int argc, char** argv) {
    using namespace gpu::systemc;

    GpuTop top("top");

    // Tiny shader: o0 = c0 + c1
    auto a = gpu::asm_::assemble("add o0, c0, c1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str());
        return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    ShaderJob job{};
    job.code = &code;
    job.is_vs = true;
    job.constants[0] = {{1.0f, 2.0f, 3.0f, 4.0f}};
    job.constants[1] = {{0.5f, 0.5f, 0.5f, 0.5f}};

    top.cp.enqueue(&job);

    sc_core::sc_start(1, sc_core::SC_MS);

    auto& o = job.outputs[0];
    bool ok = (std::fabs(o[0] - 1.5f) < 1e-5f) &&
              (std::fabs(o[1] - 2.5f) < 1e-5f) &&
              (std::fabs(o[2] - 3.5f) < 1e-5f) &&
              (std::fabs(o[3] - 4.5f) < 1e-5f);
    std::printf("SC out = (%g, %g, %g, %g)\n", o[0], o[1], o[2], o[3]);
    if (!ok) { std::fprintf(stderr, "FAIL\n"); return 1; }
    std::printf("PASS @ sim time = %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
