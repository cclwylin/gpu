add_library(gpu_compiler STATIC
    assembler/src/asm.cpp
    assembler/src/disasm.cpp
    isa_sim/src/sim.cpp
    glsl/src/glsl.cpp
)
target_include_directories(gpu_compiler PUBLIC include)

# CLI tools
add_executable(gpu-as       assembler/asm_main.cpp)
add_executable(gpu-disasm   assembler/disasm_main.cpp)
add_executable(gpu-isa-sim  isa_sim/sim_main.cpp)
add_executable(gpu-glslc    glsl/glslc_main.cpp)
foreach(t gpu-as gpu-disasm gpu-isa-sim gpu-glslc)
    target_link_libraries(${t} PRIVATE gpu_compiler)
endforeach()
