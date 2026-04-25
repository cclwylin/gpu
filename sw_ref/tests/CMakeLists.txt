add_executable(test_basic test_basic.cpp)
target_link_libraries(test_basic PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.basic COMMAND test_basic)

add_executable(test_fp test_fp.cpp)
target_link_libraries(test_fp PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.fp COMMAND test_fp)

add_executable(test_isa_triangle test_isa_triangle.cpp)
target_link_libraries(test_isa_triangle PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.isa_triangle COMMAND test_isa_triangle)

add_executable(test_msaa test_msaa.cpp)
target_link_libraries(test_msaa PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.msaa COMMAND test_msaa)

add_executable(test_texture test_texture.cpp)
target_link_libraries(test_texture PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.texture COMMAND test_texture)

add_executable(test_pfo test_pfo.cpp)
target_link_libraries(test_pfo PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.pfo COMMAND test_pfo)

add_executable(test_stencil_scissor test_stencil_scissor.cpp)
target_link_libraries(test_stencil_scissor PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.stencil_scissor COMMAND test_stencil_scissor)
