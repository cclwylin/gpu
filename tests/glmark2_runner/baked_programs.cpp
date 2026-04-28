// Sprint 36 — register hand-written ISA for the (vs,fs) source pairs
// used by the glmark2 runner scenes. Lifts the burden off the GLSL
// frontend (follow-up #7) so the rest of the pipeline can be
// exercised today.

#include "baked_programs.h"

#include "glcompat_es2.h"
#include "gpu_compiler/asm.h"

#include <cstdio>
#include <stdexcept>

namespace gpu::glmark2_runner {
namespace {

std::vector<uint64_t> assemble_or_die(const char* what, const std::string& src) {
    auto a = gpu::asm_::assemble(src);
    if (!a.error.empty()) {
        std::fprintf(stderr, "[baked %s] asm err: %s\n", what, a.error.c_str());
        throw std::runtime_error(a.error);
    }
    return std::vector<uint64_t>(a.code.begin(), a.code.end());
}

}  // namespace

const std::string kPosColorVs = R"GLSL(#version 100
attribute vec4 a_pos;
attribute vec4 a_color;
uniform mat4 u_mvp;
varying vec4 v_color;
void main() {
    gl_Position = u_mvp * a_pos;
    v_color = a_color;
}
)GLSL";

const std::string kPosColorFs = R"GLSL(#version 100
precision mediump float;
varying vec4 v_color;
void main() {
    gl_FragColor = v_color;
}
)GLSL";

void register_baked_programs() {
    using glcompat::es2::BakedProgram;
    using glcompat::es2::register_baked_program;

    BakedProgram pos_color;
    pos_color.vs_code = assemble_or_die("pos_color VS", R"ASM(
        ; gl_Position = u_mvp * a_pos
        dp4 o0.x, c0, r0
        dp4 o0.y, c1, r0
        dp4 o0.z, c2, r0
        dp4 o0.w, c3, r0
        ; v_color = a_color
        mov o1, r1
    )ASM");
    pos_color.fs_code = assemble_or_die("pos_color FS", R"ASM(
        ; gl_FragColor = v_color
        mov o0, v0
    )ASM");
    pos_color.num_attribs  = 2;
    pos_color.num_varyings = 1;
    pos_color.attrib_loc["a_pos"]   = 0;
    pos_color.attrib_loc["a_color"] = 1;
    pos_color.uniform_loc["u_mvp"]  = 0;
    pos_color.uniform_to_const_slot[0] = 0;       // c0..c3
    pos_color.uniform_slot_count[0]    = 4;       // mat4 = 4 vec4 slots
    register_baked_program(kPosColorVs, kPosColorFs, std::move(pos_color));
}

}  // namespace gpu::glmark2_runner
