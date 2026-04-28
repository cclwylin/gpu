#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "encoding.h"

namespace gpu::asm_ {

struct AssembleResult {
    std::vector<isa::Inst> code;
    std::string error;          // empty on success
    int error_line = 0;
};

// Parse a textual .asm program (per docs/isa_spec.md §8) and emit 64-bit
// instructions. Errors are reported with the offending source line.
AssembleResult assemble(const std::string& source);

// Render a binary back to canonical text (full xyzw swizzles).
std::string disassemble(const std::vector<isa::Inst>& code);

}  // namespace gpu::asm_
