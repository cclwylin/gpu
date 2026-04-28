// gpu-isa-sim: run a shader binary, print outputs / a few GPRs.
//
// Inputs are seeded simply via env vars in this Sprint 1 stub. Real test
// drivers will use the library API directly.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#include "gpu_compiler/sim.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <shader.bin>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    const auto sz = in.tellg();
    in.seekg(0);
    std::vector<gpu::isa::Inst> code(sz / sizeof(gpu::isa::Inst));
    in.read(reinterpret_cast<char*>(code.data()), sz);

    gpu::sim::ThreadState t{};
    auto r = gpu::sim::execute(code, t);
    if (!r.ok) { std::fprintf(stderr, "exec error: %s\n", r.error.c_str()); return 1; }

    for (int i = 0; i < 4; ++i) {
        std::printf("o%d = (%g, %g, %g, %g)\n", i,
                    t.o[i][0], t.o[i][1], t.o[i][2], t.o[i][3]);
    }
    return 0;
}
