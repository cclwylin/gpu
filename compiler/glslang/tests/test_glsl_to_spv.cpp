// Smoke: compile a tiny GLSL fragment via glslang, verify SPIR-V magic word.
#include <cstdio>

#include "gpu_glslang/glsl_to_spv.h"

int main() {
    const char* glsl =
        "#version 100\n"
        "precision mediump float;\n"
        "varying vec4 v_color;\n"
        "void main() { gl_FragColor = v_color; }\n";

    auto r = gpu::glslang_fe::compile(glsl, gpu::glslang_fe::Stage::Fragment);
    if (!r.error.empty()) {
        std::fprintf(stderr, "FAIL %s\n%s\n", r.error.c_str(), r.info_log.c_str());
        return 1;
    }
    if (r.spirv.size() < 5) {
        std::fprintf(stderr, "FAIL: SPIR-V too short (%zu words)\n", r.spirv.size());
        return 1;
    }
    if (r.spirv[0] != 0x07230203u) {     // SPIR-V magic
        std::fprintf(stderr, "FAIL: bad SPIR-V magic 0x%08x\n", r.spirv[0]);
        return 1;
    }
    std::printf("PASS — %zu SPIR-V words\n", r.spirv.size());
    return 0;
}
