add_executable(test_asm_roundtrip test_asm_roundtrip.cpp)
target_link_libraries(test_asm_roundtrip PRIVATE gpu_compiler)
add_test(NAME compiler.asm_roundtrip COMMAND test_asm_roundtrip)

add_executable(test_sim_basic test_sim_basic.cpp)
target_link_libraries(test_sim_basic PRIVATE gpu_compiler)
add_test(NAME compiler.sim_basic COMMAND test_sim_basic)

add_executable(test_glsl_compile test_glsl_compile.cpp)
target_link_libraries(test_glsl_compile PRIVATE gpu_compiler)
add_test(NAME compiler.glsl_compile COMMAND test_glsl_compile)

add_executable(test_sim_warp test_sim_warp.cpp)
target_link_libraries(test_sim_warp PRIVATE gpu_compiler)
add_test(NAME compiler.sim_warp COMMAND test_sim_warp)

add_executable(test_warp_break test_warp_break.cpp)
target_link_libraries(test_warp_break PRIVATE gpu_compiler)
add_test(NAME compiler.warp_break COMMAND test_warp_break)

add_executable(test_glsl_ext test_glsl_ext.cpp)
target_link_libraries(test_glsl_ext PRIVATE gpu_compiler)
add_test(NAME compiler.glsl_ext COMMAND test_glsl_ext)

add_executable(test_spv_lower test_spv_lower.cpp)
target_link_libraries(test_spv_lower PRIVATE gpu_compiler)
add_test(NAME compiler.spv_lower COMMAND test_spv_lower)
