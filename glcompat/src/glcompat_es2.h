// Sprint 36 — internal helper exposed by glcompat_es2.cpp so runner
// code (`tests/glmark2_runner/`) can register pre-baked ISA programs
// keyed by (vs_src, fs_src). The hand-rolled GLSL parser in
// `compiler/glsl/` doesn't yet cover the full glmark2 shader set
// (follow-up #7), so the runner provides the lowering up-front.
//
// Each baked entry maps:
//   - GL attrib name → ABI attrib index (0..7)
//   - GL uniform name → abstract location (later resolved by the
//     runner to a (slot, vec4_index) pair via uniform_to_const_slot).
//
// Once registered, a normal glCreateShader/glShaderSource/
// glCompileShader/glAttachShader/glLinkProgram sequence with matching
// sources picks the baked entry and the program is fully operational.

#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace glcompat::es2 {

struct BakedProgram {
    std::vector<uint64_t> vs_code;
    std::vector<uint64_t> fs_code;

    int                                       num_attribs = 0;
    int                                       num_varyings = 0;
    std::unordered_map<std::string, int>      attrib_loc;       // name -> ABI attrib index
    std::unordered_map<std::string, int>      uniform_loc;      // name -> abstract loc id (>=0)
    // Each uniform location consumes one or more vec4 c-bank slots
    // (mat4 = 4 slots; vec4/vec3/vec2/float = 1 slot each).
    std::unordered_map<int, int>              uniform_to_const_slot;
    std::unordered_map<int, int>              uniform_slot_count;   // mat4=4, else 1

    // Compiler-emitted float literals: load `value` into c[slot].x at
    // draw time (the rest of the vec4 stays at zero). Sprint 37.
    std::vector<std::pair<int, float>>        literals;

    // Sampler bindings (Sprint 38). For each sampler uniform location
    // (already present in uniform_loc), this map says which ISA `tex N`
    // slot the shader will read from. The user-supplied unit number
    // (set via glUniform1i) selects which es2_tex_units[unit] the host
    // forwards to ctx.textures[N].
    std::unordered_map<int, int>              sampler_to_tex_slot;
};

// Returns a non-zero token if newly registered, 0 if a duplicate
// (vs,fs) was already present (the existing entry is kept).
int register_baked_program(const std::string& vs_src,
                           const std::string& fs_src,
                           BakedProgram baked);

// Look up a baked entry by source. Returns -1 if no match.
int find_baked_program(const std::string& vs_src,
                       const std::string& fs_src);

// Accessor used by the draw path.
const BakedProgram* baked_program(int id);

}  // namespace glcompat::es2
