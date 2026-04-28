# Sprint 12 — glslang frontend (gated).
#
# When -DGPU_BUILD_GLSLANG=ON, fetches glslang at the version pinned by
# third_party/versions.yaml and builds:
#   - libgpu_glslang_frontend  (wraps glslang::TShader)
#   - gpu-glsl-to-spv          (CLI: GLSL -> SPIR-V .spv)
#
# Disabled by default because cold-fetching + building glslang takes ~10
# minutes. Enabled in Docker / CI where the build cost is amortised.
#
# Out of scope for Sprint 12: SPIR-V -> our IR lowering. The wrapper here
# proves the integration is live (we can produce SPIR-V from arbitrary
# ES 2.0 GLSL); a future sprint adds the lowering pass that consumes the
# SPIR-V and feeds the existing codegen, replacing the hand-written
# parser in compiler/glsl/.

option(GPU_BUILD_GLSLANG "Fetch + build glslang frontend" OFF)
if(NOT GPU_BUILD_GLSLANG)
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG        14.0.0
    GIT_SHALLOW    TRUE
)
# Disable optional / costly bits before MakeAvailable.
set(BUILD_TESTING                 OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES       OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL                   OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER            OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT                    OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS                 OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glslang)

add_library(gpu_glslang_frontend STATIC src/glsl_to_spv.cpp)
target_include_directories(gpu_glslang_frontend PUBLIC include)
target_link_libraries(gpu_glslang_frontend
    PUBLIC  glslang SPIRV)

add_executable(gpu-glsl-to-spv glsl_to_spv_main.cpp)
target_link_libraries(gpu-glsl-to-spv PRIVATE gpu_glslang_frontend)

if(GPU_BUILD_TESTS)
    add_executable(test_glsl_to_spv tests/test_glsl_to_spv.cpp)
    target_link_libraries(test_glsl_to_spv PRIVATE gpu_glslang_frontend)
    add_test(NAME compiler.glsl_to_spv COMMAND test_glsl_to_spv)
endif()
