#include "gpu_glslang/glsl_to_spv.h"

// Minimal glslang invocation. Only the bits we need:
//   TShader / TProgram / parse + link / GlslangToSpv.
// All resource limits taken from glslang's "default" example settings
// (DefaultTBuiltInResource is shipped with glslang in newer versions; we
// hard-code a permissive set here to avoid pulling in StandAlone/).
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

namespace gpu::glslang_fe {

namespace {

const TBuiltInResource& default_resources() {
    static TBuiltInResource r{};
    static bool init = false;
    if (init) return r;
    // Permissive limits — sufficient for ES 2.0 subset shaders.
    r.maxLights = 32; r.maxClipPlanes = 6; r.maxTextureUnits = 32;
    r.maxTextureCoords = 32; r.maxVertexAttribs = 64;
    r.maxVertexUniformComponents = 4096;
    r.maxVaryingFloats = 64;
    r.maxVertexTextureImageUnits = 32;
    r.maxCombinedTextureImageUnits = 80;
    r.maxTextureImageUnits = 32;
    r.maxFragmentUniformComponents = 4096;
    r.maxDrawBuffers = 32;
    r.maxVertexUniformVectors = 128;
    r.maxVaryingVectors = 8;
    r.maxFragmentUniformVectors = 16;
    r.maxVertexOutputVectors = 16;
    r.maxFragmentInputVectors = 15;
    r.minProgramTexelOffset = -8; r.maxProgramTexelOffset = 7;
    r.limits.nonInductiveForLoops = 1;
    r.limits.whileLoops = 1;
    r.limits.doWhileLoops = 1;
    r.limits.generalUniformIndexing = 1;
    r.limits.generalAttributeMatrixVectorIndexing = 1;
    r.limits.generalVaryingIndexing = 1;
    r.limits.generalSamplerIndexing = 1;
    r.limits.generalVariableIndexing = 1;
    r.limits.generalConstantMatrixVectorIndexing = 1;
    init = true;
    return r;
}

bool g_initialised = false;
void ensure_init() {
    if (!g_initialised) {
        glslang::InitializeProcess();
        g_initialised = true;
    }
}

}  // namespace

SpvResult compile(const std::string& glsl, Stage stage) {
    ensure_init();
    SpvResult res;

    EShLanguage lang = (stage == Stage::Vertex)
                        ? EShLangVertex : EShLangFragment;
    glslang::TShader shader(lang);
    const char* sources[1] = { glsl.c_str() };
    shader.setStrings(sources, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, lang,
                       glslang::EShClientOpenGL, 100);
    shader.setEnvClient(glslang::EShClientOpenGL,
                        glslang::EShTargetOpenGL_450);
    shader.setEnvTarget(glslang::EShTargetSpv,
                        glslang::EShTargetSpv_1_0);

    const auto& res_limits = default_resources();
    if (!shader.parse(&res_limits, 100, false, EShMsgDefault)) {
        res.error = "glslang parse failed";
        res.info_log = std::string(shader.getInfoLog()) + "\n" +
                       shader.getInfoDebugLog();
        return res;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        res.error = "glslang link failed";
        res.info_log = std::string(program.getInfoLog()) + "\n" +
                       program.getInfoDebugLog();
        return res;
    }
    glslang::GlslangToSpv(*program.getIntermediate(lang), res.spirv);
    res.info_log = shader.getInfoLog();
    return res;
}

}  // namespace gpu::glslang_fe
