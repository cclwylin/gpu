// gpu-as: assemble a .asm into a .bin (raw 64-bit instructions, little-endian).
#include <cstdio>
#include <fstream>
#include <sstream>

#include "gpu_compiler/asm.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <input.asm> <output.bin>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    std::ostringstream buf; buf << in.rdbuf();

    auto r = gpu::asm_::assemble(buf.str());
    if (!r.error.empty()) {
        std::fprintf(stderr, "asm error at %s:%d: %s\n", argv[1], r.error_line, r.error.c_str());
        return 1;
    }

    std::ofstream out(argv[2], std::ios::binary);
    out.write(reinterpret_cast<const char*>(r.code.data()),
              static_cast<std::streamsize>(r.code.size() * sizeof(gpu::isa::Inst)));
    std::fprintf(stderr, "%zu instructions written to %s\n", r.code.size(), argv[2]);
    return 0;
}
