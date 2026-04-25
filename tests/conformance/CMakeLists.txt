add_executable(scene_runner scene_runner.cpp)
target_link_libraries(scene_runner PRIVATE gpu_sw_ref)

# Walk the tests/scenes/ tree and add one CTest per .scene file.
file(GLOB SCENE_FILES "${CMAKE_SOURCE_DIR}/tests/scenes/*.scene")
foreach(scene_path ${SCENE_FILES})
    get_filename_component(scene_name ${scene_path} NAME_WE)
    add_test(NAME conformance.${scene_name}
             COMMAND scene_runner ${scene_path})
endforeach()

# Sprint 34 — SystemC pattern runner.
# Sprint 35 — gated by GPU_SYSTEMC_SKIP_TESTS exported from systemc/
# (which itself respects the GPU_FORCE_SYSTEMC_TESTS opt-in flag).
if(GPU_BUILD_SYSTEMC AND TARGET gpu_systemc AND NOT GPU_SYSTEMC_SKIP_TESTS)
    add_executable(sc_pattern_runner sc_pattern_runner.cpp)
    target_link_libraries(sc_pattern_runner PRIVATE gpu_systemc)
    set_target_properties(sc_pattern_runner PROPERTIES
        INSTALL_RPATH "${GPU_SYSTEMC_LIB_DIR}"
        BUILD_RPATH   "${GPU_SYSTEMC_LIB_DIR}")
endif()
