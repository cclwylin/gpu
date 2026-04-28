// Round-trip test: assemble a small canonical program, disassemble, re-assemble,
// and assert binary equality. Catches encode/decode asymmetry.

#include <cstdio>
#include <cstring>

#include "gpu_compiler/asm.h"

int main() {
    // NB: ISA encoding constrains mad/cmp src2 to GPR (no class field for src2).
    // So we use r2 as src2 below rather than a constant.
    const std::string src =
        "mul  r1.xyz, r0.xyz, c0.xxxx\n"
        "mad  r2,     r1,     c1, r2\n"
        "dp4  o0.x,   c0,     r0\n"
        "dp3  r0.x,   v1.xyz, v1.xyz\n"
        "dp2  r0.x,   r0.xy,  r0.xy\n"
        "rsq  r3,     r2\n"
        "tex  r4,     v0.xy,  tex0\n"
        "mov  o0,     r4\n"
        "setp_lt p,   r0.x,   c4.xxxx\n"
        "(p) kil\n"
        "loop 32\n"
        "  setp_ge p, r0.x, c1.xxxx\n"
        "  (p) break\n"
        "  add r0.x, r0.x, c3.xxxx\n"
        "endloop\n";

    auto a1 = gpu::asm_::assemble(src);
    if (!a1.error.empty()) {
        std::fprintf(stderr, "assemble #1 failed at line %d: %s\n",
                     a1.error_line, a1.error.c_str());
        return 1;
    }
    auto text2 = gpu::asm_::disassemble(a1.code);

    auto a2 = gpu::asm_::assemble(text2);
    if (!a2.error.empty()) {
        std::fprintf(stderr, "assemble #2 (after disasm) failed at line %d: %s\nText was:\n%s\n",
                     a2.error_line, a2.error.c_str(), text2.c_str());
        return 1;
    }

    if (a1.code.size() != a2.code.size()) {
        std::fprintf(stderr, "FAIL: instruction count differs %zu vs %zu\n",
                     a1.code.size(), a2.code.size());
        return 1;
    }
    for (size_t i = 0; i < a1.code.size(); ++i) {
        if (a1.code[i] != a2.code[i]) {
            std::fprintf(stderr, "FAIL: inst[%zu] differs 0x%016llx vs 0x%016llx\n",
                         i, (unsigned long long)a1.code[i], (unsigned long long)a2.code[i]);
            return 1;
        }
    }
    std::printf("PASS roundtrip (%zu insts)\n", a1.code.size());
    return 0;
}
