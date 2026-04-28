// gpu-disasm: dump a .bin as canonical text.
#include <cstdio>
#include <fstream>
#include <vector>

#include "gpu_compiler/asm.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <input.bin> [output.asm]\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    const auto sz = in.tellg();
    in.seekg(0);
    std::vector<gpu::isa::Inst> code(sz / sizeof(gpu::isa::Inst));
    in.read(reinterpret_cast<char*>(code.data()), sz);

    auto text = gpu::asm_::disassemble(code);
    if (argc >= 3) {
        std::ofstream out(argv[2]);
        out << text;
    } else {
        std::fputs(text.c_str(), stdout);
    }
    return 0;
}
