add_library(gpu_sw_ref STATIC
    src/fp/fp32.cpp
    src/types/math.cpp
    src/state/context.cpp
    src/pipeline/vertex_fetch.cpp
    src/pipeline/vertex_shader.cpp
    src/pipeline/primitive_assembly.cpp
    src/pipeline/rasterizer.cpp
    src/pipeline/fragment_shader.cpp
    src/pipeline/per_fragment_ops.cpp
    src/pipeline/resolve.cpp
    src/pipeline/pipeline.cpp
    src/trace/trace.cpp
    src/gl_api/gl_api.cpp
)

target_include_directories(gpu_sw_ref PUBLIC include)

# Drivers / tools that link to the model
add_executable(sw_ref_smoke src/main_smoke.cpp)
target_link_libraries(sw_ref_smoke PRIVATE gpu_sw_ref)
