// Sprint 13 SPIR-V → ISA smoke. Hand-crafts a minimal SPIR-V module that
// represents:
//
//   uniform mat4 u_mvp;
//   in vec4 a_pos;
//   out vec4 gl_Position;
//   void main() { gl_Position = u_mvp * a_pos; }
//
// Then runs lower() and asserts:
//   - lowering succeeds (no error)
//   - 4 dp4 instructions emitted (one per matrix row)
//   - 1 mov from GPR -> output o0
//   - ABI metadata: 1 attribute, 1 uniform (mat4 -> 4 c-bank slots)
//
// This validates the parser without needing glslang at build time.

#include <cstdio>
#include <cstring>
#include <vector>

#include "gpu_compiler/encoding.h"
#include "gpu_spv/spv_to_isa.h"

namespace {
int fails = 0;
#define EXPECT(cond) do {                                                    \
    if (!(cond)) {                                                           \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++fails;                                                             \
    }                                                                        \
} while (0)

uint32_t inst(uint16_t op, uint16_t len) {
    return (static_cast<uint32_t>(len) << 16) | op;
}
}  // namespace

int main() {
    // Build the SPIR-V bytestream by hand.
    //
    // Id allocation:
    //   1: void    2: float  3: vec4   4: mat4
    //   5: mat4*   6: vec4_in*    7: vec4_out*
    //   8: u_mvp (Variable, mat4*, Uniform)
    //   9: a_pos  (Variable, vec4*, Input)
    //  10: gl_Position (Variable, vec4*, Output)
    //  11: function type (-> void)
    //  12: main function
    //  13: label
    //  14: load mat from u_mvp
    //  15: load vec from a_pos
    //  16: mat * vec
    //  --- store gl_Position
    //
    // This is the bare-minimum structure spv_to_isa needs.
    std::vector<uint32_t> w = {
        0x07230203,                  // magic
        0x00010000,                  // version 1.0
        0,                           // generator
        20,                          // bound
        0,                           // schema
    };

    // OpName: u_mvp, a_pos, gl_Position
    auto add_name = [&](uint32_t id, const char* name) {
        size_t name_len = std::strlen(name) + 1;          // include nul
        size_t name_words = (name_len + 3) / 4;
        uint16_t total = static_cast<uint16_t>(2 + name_words);
        w.push_back(inst(5 /*OpName*/, total));
        w.push_back(id);
        std::vector<char> buf(name_words * 4, 0);
        std::memcpy(buf.data(), name, name_len);
        for (size_t i = 0; i < name_words; ++i) {
            uint32_t word; std::memcpy(&word, &buf[i * 4], 4);
            w.push_back(word);
        }
    };
    add_name(8,  "u_mvp");
    add_name(9,  "a_pos");
    add_name(10, "gl_Position");

    // Type declarations.
    w.push_back(inst(19 /*OpTypeVoid*/,  2)); w.push_back(1);
    w.push_back(inst(22 /*OpTypeFloat*/, 3)); w.push_back(2); w.push_back(32);
    w.push_back(inst(23 /*OpTypeVector*/,4)); w.push_back(3); w.push_back(2); w.push_back(4);
    w.push_back(inst(24 /*OpTypeMatrix*/,4)); w.push_back(4); w.push_back(3); w.push_back(4);
    // Pointer types: mat4* (Uniform=2), vec4* (Input=1), vec4* (Output=3)
    w.push_back(inst(32 /*OpTypePointer*/,4)); w.push_back(5); w.push_back(2); w.push_back(4);
    w.push_back(inst(32, 4));                  w.push_back(6); w.push_back(1); w.push_back(3);
    w.push_back(inst(32, 4));                  w.push_back(7); w.push_back(3); w.push_back(3);

    // Variable declarations
    w.push_back(inst(59 /*OpVariable*/, 4)); w.push_back(5); w.push_back(8);  w.push_back(2);   // u_mvp
    w.push_back(inst(59, 4));                w.push_back(6); w.push_back(9);  w.push_back(1);   // a_pos
    w.push_back(inst(59, 4));                w.push_back(7); w.push_back(10); w.push_back(3);   // gl_Position

    // OpDecorate gl_Position BuiltIn Position(0)
    w.push_back(inst(71 /*OpDecorate*/, 4));
    w.push_back(10); w.push_back(11 /*BuiltIn*/); w.push_back(0 /*Position*/);

    // Function type: void()
    w.push_back(inst(33 /*OpTypeFunction*/, 3)); w.push_back(11); w.push_back(1);
    // Function start
    w.push_back(inst(54 /*OpFunction*/, 5));
    w.push_back(1); w.push_back(12); w.push_back(0); w.push_back(11);
    w.push_back(inst(248 /*OpLabel*/, 2)); w.push_back(13);
    // Load matrix and vector
    w.push_back(inst(61 /*OpLoad*/, 4)); w.push_back(4);  w.push_back(14); w.push_back(8);  // mat4 = load u_mvp
    w.push_back(inst(61,             4)); w.push_back(3);  w.push_back(15); w.push_back(9);  // vec4 = load a_pos
    // OpMatrixTimesVector
    w.push_back(inst(145 /*OpMatrixTimesVector*/, 5));
    w.push_back(3); w.push_back(16); w.push_back(14); w.push_back(15);
    // OpStore gl_Position
    w.push_back(inst(62 /*OpStore*/, 3)); w.push_back(10); w.push_back(16);
    w.push_back(inst(253 /*OpReturn*/,  1));
    w.push_back(inst(56  /*OpFunctionEnd*/, 1));

    auto r = gpu::spv::lower(w, gpu::spv::Stage::Vertex);
    if (!r.error.empty()) {
        std::fprintf(stderr, "FAIL: lower error: %s\n", r.error.c_str());
        return 1;
    }

    // Count emitted instructions by opcode.
    int dp4 = 0, mov = 0;
    for (auto inst_w : r.code) {
        gpu::isa::AluFields af = gpu::isa::decode_alu(inst_w);
        if (af.op == 0x06) ++dp4;
        if (af.op == 0x01) ++mov;
    }
    std::printf("emitted insts=%zu  dp4=%d  mov=%d\n", r.code.size(), dp4, mov);

    EXPECT(r.attributes.size() == 1);
    EXPECT(r.uniforms.size()   == 1);
    EXPECT(r.uniforms[0].slot  == 0);     // mat4 starts at c0
    EXPECT(dp4 == 4);                     // one dp4 per matrix row
    EXPECT(mov >= 1);                     // at least the OpStore lowering

    if (fails) { std::fprintf(stderr, "FAIL %d\n", fails); return 1; }
    std::printf("PASS\n");
    return 0;
}
