add_executable(scene_runner scene_runner.cpp)
target_link_libraries(scene_runner PRIVATE gpu_sw_ref)

# Walk the tests/scenes/ tree and add one CTest per .scene file.
file(GLOB SCENE_FILES "${CMAKE_SOURCE_DIR}/tests/scenes/*.scene")
list(FILTER SCENE_FILES EXCLUDE REGEX "/\\._")    # drop macOS AppleDouble shadows
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

    # Sprint 41 — SC ↔ sw_ref bit-parity track. For each scene that ships
    # a golden PPM (rendered by sw_ref via `scene_runner --write-golden`),
    # register a `conformance.<name>.sc` ctest that drives the cycle-
    # accurate chain through the same vertices and asserts the result
    # matches the golden within the project-wide 1.0 RMSE budget.
    foreach(scene_path ${SCENE_FILES})
        get_filename_component(scene_name ${scene_path} NAME_WE)
        set(_golden "${CMAKE_SOURCE_DIR}/tests/scenes/${scene_name}.golden.ppm")
        if(EXISTS "${_golden}")
            add_test(NAME conformance.${scene_name}.sc
                     COMMAND sc_pattern_runner
                             ${scene_path}
                             ${CMAKE_CURRENT_BINARY_DIR}/${scene_name}.sc.ppm
                             --ref     ${_golden}
                             --rmse-max 1.0)
        endif()
    endforeach()
endif()

# Sprint 42 — VK-GL-CTS (Khronos OpenGL ES 2.0 CTS) entry. The CTS binary is
# built outside this tree by tools/build_vkglcts.sh, which lives in its own
# build dir (build_vkglcts/) because CTS ships its own cmake project that
# wants to own everything from spirv-headers to its own libpng. Here we just
# register a wrapper that runs the prebuilt binary against a curated case
# filter and asserts the pass count matches today's baseline.
if(GPU_BUILD_VKGLCTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(_vkglcts_bin "${CMAKE_SOURCE_DIR}/build_vkglcts/modules/gles2/deqp-gles2")
    set(_vkglcts_runner "${CMAKE_SOURCE_DIR}/tools/run_vkglcts.py")

    # color_clear group — Sprint 43 closed the scissor + glColorMask gaps,
    # bringing us to 19 / 19 PASS. Hold the bar at 19 so any drop registers
    # as a regression.
    add_test(NAME vkglcts.gles2.color_clear
             COMMAND ${Python3_EXECUTABLE} ${_vkglcts_runner}
                     "dEQP-GLES2.functional.color_clear.*"
                     --bin        ${_vkglcts_bin}
                     --min-passes 19
                     --total      19)
    # implementation_limits group — Sprint 44 closed every GL_MAX_* /
    # aliased range / shader-compiler query in glcompat::glGet*v.
    add_test(NAME vkglcts.gles2.implementation_limits
             COMMAND ${Python3_EXECUTABLE} ${_vkglcts_runner}
                     "dEQP-GLES2.functional.implementation_limits.*"
                     --bin        ${_vkglcts_bin}
                     --min-passes 16
                     --total      16)
    if(NOT EXISTS "${_vkglcts_bin}")
        message(STATUS
            "GPU_BUILD_VKGLCTS=ON but ${_vkglcts_bin} not found — "
            "run tools/build_vkglcts.sh; the test will pick it up at ctest time.")
    endif()
endif()
