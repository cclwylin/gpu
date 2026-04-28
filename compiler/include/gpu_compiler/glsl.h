#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "encoding.h"

// Minimal GLSL ES 2.0 subset compiler.
//
// Covers what's needed to compile tests/shader_corpus/ref_shader_1/*.glsl:
//  - Top-level decls: attribute / uniform / varying / sampler2D
//  - Types: vec2 / vec3 / vec4 / mat4 / float / sampler2D
//  - Single function: void main()
//  - Statements: assignment to gl_Position / built-in or declared output
//  - Expressions: identifier, member access (.xyz), binary * +, function call
//  - Functions: dot, normalize (Sprint 2.1+), texture2D
//
// Out of scope this sprint: control flow, max/min/clamp, ternary, scalar
// arithmetic on locals — added incrementally per benchmark needs.

namespace gpu::glsl {

enum class ShaderStage { Vertex, Fragment };

struct CompileResult {
    std::vector<isa::Inst> code;

    // ABI metadata so the host can wire up uniforms / attributes / varyings.
    struct Binding {
        std::string name;
        std::string type;     // "vec4", "mat4", ...
        int slot = -1;        // for attributes / uniforms / varyings
    };
    std::vector<Binding> attributes;     // VS: a_pos / a_color / a_uv ...
    std::vector<Binding> uniforms;       // c0..c15 packing
    std::vector<Binding> varyings_out;   // VS: o1..o7
    std::vector<Binding> varyings_in;    // FS: v0..v7
    std::vector<Binding> samplers;       // tex0..tex15

    // Compiler-managed float literal pool. Each entry says "load
    // `value` into c[slot].x at draw time"; the host (glcompat /
    // runner) is responsible for writing it into ctx.draw.uniforms.
    // Sprint 9 reserves these from the top of the const bank, growing
    // downward.
    struct Literal { int slot; float value; };
    std::vector<Literal> literals;

    std::string error;
    int error_line = 0;
};

// Sprint 55+56 — `uniform_slot_base` lets the host (glcompat) compile
// VS and FS into a *shared* c-bank without colliding. `literal_slot_top`
// is where FS literals grow down from. `preset_literals` lets FS reuse
// VS's already-assigned literal slots when the value matches — without
// dedup, the c-bank's 16 slots overflow on shaders with many distinct
// literals. Default values keep single-stage call sites unchanged.
struct LiteralBinding { int slot; float value; };
CompileResult compile(const std::string& source, ShaderStage stage,
                      int uniform_slot_base = 0,
                      int literal_slot_top  = 32,    // Sprint 56 — c-bank grew 16 → 32
                      const std::vector<LiteralBinding>& preset_literals = {});

}  // namespace gpu::glsl
