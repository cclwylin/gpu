// gpu-glsl-to-spv: compile a GLSL ES 2.0 source file to SPIR-V (.spv).
// Usage: gpu-glsl-to-spv {-vs|-fs} input.glsl output.spv

#include <cstdio>
#include <fstream>
#include <sstream>

#include "gpu_glslang/glsl_to_spv.h"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s {-vs|-fs} input.glsl output.spv\n", argv[0]);
        return 2;
    }
    std::string flag = argv[1];
    auto stage = (flag == "-vs") ? gpu::glslang_fe::Stage::Vertex
               : (flag == "-fs") ? gpu::glslang_fe::Stage::Fragment
                                  : gpu::glslang_fe::Stage::Vertex;
    if (flag != "-vs" && flag != "-fs") {
        std::fprintf(stderr, "first arg must be -vs or -fs\n"); return 2;
    }
    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 1; }
    std::ostringstream buf; buf << in.rdbuf();

    auto r = gpu::glslang_fe::compile(buf.str(), stage);
    if (!r.error.empty()) {
        std::fprintf(stderr, "%s: %s\n%s\n",
                     r.error.c_str(), argv[2], r.info_log.c_str());
        return 1;
    }
    std::ofstream out(argv[3], std::ios::binary);
    out.write(reinterpret_cast<const char*>(r.spirv.data()),
              static_cast<std::streamsize>(r.spirv.size() * sizeof(uint32_t)));
    std::fprintf(stderr, "%zu SPIR-V words written to %s\n",
                 r.spirv.size(), argv[3]);
    return 0;
}
