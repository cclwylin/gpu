#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace gpu::glslang_fe {

enum class Stage { Vertex, Fragment };

struct SpvResult {
    std::vector<uint32_t> spirv;     // SPIR-V words (4 bytes each)
    std::string           info_log;
    std::string           error;     // empty on success
};

// Compile a GLSL ES 2.0 source string to SPIR-V using glslang.
// Returns SpvResult with `error` non-empty if compilation fails.
SpvResult compile(const std::string& glsl, Stage stage);

}  // namespace gpu::glslang_fe
