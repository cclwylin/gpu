add_executable(test_basic test_basic.cpp)
target_link_libraries(test_basic PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.basic COMMAND test_basic)

add_executable(test_fp test_fp.cpp)
target_link_libraries(test_fp PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.fp COMMAND test_fp)

add_executable(test_isa_triangle test_isa_triangle.cpp)
target_link_libraries(test_isa_triangle PRIVATE gpu_sw_ref)
add_test(NAME sw_ref.isa_triangle COMMAND test_isa_triangle)
