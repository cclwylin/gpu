#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "gpu_compiler/encoding.h"

namespace gpu::spv {

// Sprint 13 minimal SPIR-V → ISA lowering.
//
// Pure SPIR-V parser (no glslang dependency); always built into gpu_compiler.
// glslang produces SPIR-V; we read the bytes and emit our 64-bit ISA. Subset
// matches the patterns of test_glsl_compile.cpp's reference shader
// (`gl_Position = u_mvp * a_pos; v = a * u`). Anything outside that returns
// `error` non-empty — opcode coverage widens incrementally per workload need.

struct LowerResult {
    std::vector<isa::Inst> code;

    struct Binding { std::string name; std::string type; int slot = -1; };
    std::vector<Binding> attributes;
    std::vector<Binding> uniforms;
    std::vector<Binding> varyings_out;
    std::vector<Binding> varyings_in;
    std::vector<Binding> samplers;

    std::string error;
};

enum class Stage { Vertex, Fragment };

LowerResult lower(const std::vector<uint32_t>& spirv, Stage stage);

}  // namespace gpu::spv
