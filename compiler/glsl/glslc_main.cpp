// gpu-glslc — minimal GLSL ES 2.0 subset compiler.
// Usage: gpu-glslc {-vs|-fs} <input.glsl> <output.bin>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu_compiler/glsl.h"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s {-vs|-fs} <input.glsl> <output.bin>\n", argv[0]);
        return 2;
    }
    std::string flag = argv[1];
    auto stage = (flag == "-vs") ? gpu::glsl::ShaderStage::Vertex
               : (flag == "-fs") ? gpu::glsl::ShaderStage::Fragment
                                 : gpu::glsl::ShaderStage::Vertex;
    if (flag != "-vs" && flag != "-fs") {
        std::fprintf(stderr, "first arg must be -vs or -fs\n"); return 2;
    }

    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 1; }
    std::ostringstream buf; buf << in.rdbuf();

    auto r = gpu::glsl::compile(buf.str(), stage);
    if (!r.error.empty()) {
        std::fprintf(stderr, "glsl error at %s:%d: %s\n",
                     argv[2], r.error_line, r.error.c_str());
        return 1;
    }

    std::ofstream out(argv[3], std::ios::binary);
    out.write(reinterpret_cast<const char*>(r.code.data()),
              static_cast<std::streamsize>(r.code.size() * sizeof(gpu::isa::Inst)));

    std::fprintf(stderr, "%zu inst written; %zu attrs / %zu uniforms / %zu varyings_out / %zu varyings_in / %zu samplers\n",
                 r.code.size(), r.attributes.size(), r.uniforms.size(),
                 r.varyings_out.size(), r.varyings_in.size(), r.samplers.size());
    return 0;
}
