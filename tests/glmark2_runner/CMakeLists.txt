# Sprint 36 — glmark2 runner scaffold (follow-ups #6/#8/#9).
#
# Two stand-alone CTest binaries that exercise the new ES 2.0 surface
# in glcompat. They do NOT compile glmark2's main.cpp / Canvas /
# scene-foo.cpp directly — those drag in EGL / GLX / X11 / Wayland.
# Instead we build minimal scene replicas that drive glcompat the same
# way real glmark2 code would.

add_library(gpu_glmark2_runner STATIC
    baked_programs.cpp
)
target_include_directories(gpu_glmark2_runner PUBLIC .)
target_include_directories(gpu_glmark2_runner PRIVATE
    "${CMAKE_SOURCE_DIR}/glcompat/src")
target_link_libraries(gpu_glmark2_runner PUBLIC gpu_glcompat gpu_compiler)

if(GPU_BUILD_TESTS)
    # Sprint 39 — runner tests dump their readback into ${ROOT}/out/
    # for visual inspection (alongside the existing legacy `glex_*`
    # PPMs). Make the directory and pass it via env so the tests use
    # an absolute path regardless of the ctest working directory.
    set(_glmark2_out_dir "${CMAKE_SOURCE_DIR}/out")
    file(MAKE_DIRECTORY "${_glmark2_out_dir}")

    add_executable(test_glmark2_clear scene_clear.cpp)
    target_link_libraries(test_glmark2_clear PRIVATE gpu_glmark2_runner)
    add_test(NAME glmark2.clear COMMAND test_glmark2_clear)
    set_tests_properties(glmark2.clear PROPERTIES
        ENVIRONMENT "GLMARK2_OUT_DIR=${_glmark2_out_dir}")

    add_executable(test_glmark2_triangle scene_triangle.cpp)
    target_link_libraries(test_glmark2_triangle PRIVATE gpu_glmark2_runner)
    add_test(NAME glmark2.triangle COMMAND test_glmark2_triangle)
    set_tests_properties(glmark2.triangle PROPERTIES
        ENVIRONMENT "GLMARK2_OUT_DIR=${_glmark2_out_dir}")

    # Sprint 37 — exercises the GLSL → ISA frontend fallback inside
    # glcompat::es2::glLinkProgram (no baked program registered).
    add_executable(test_glmark2_glsl scene_glsl.cpp)
    target_link_libraries(test_glmark2_glsl PRIVATE gpu_glmark2_runner)
    add_test(NAME glmark2.glsl COMMAND test_glmark2_glsl)
    set_tests_properties(glmark2.glsl PROPERTIES
        ENVIRONMENT "GLMARK2_OUT_DIR=${_glmark2_out_dir}")

    # Sprint 38 — multi-unit ES 2.0 texture path (sampler2D).
    add_executable(test_glmark2_textured_quad scene_textured_quad.cpp)
    target_link_libraries(test_glmark2_textured_quad PRIVATE gpu_glmark2_runner)
    add_test(NAME glmark2.textured_quad COMMAND test_glmark2_textured_quad)
    set_tests_properties(glmark2.textured_quad PROPERTIES
        ENVIRONMENT "GLMARK2_OUT_DIR=${_glmark2_out_dir}")

    # Sprint 38 — FBO render-to-texture + 2-pass sample.
    add_executable(test_glmark2_fbo scene_fbo.cpp)
    target_link_libraries(test_glmark2_fbo PRIVATE gpu_glmark2_runner)
    add_test(NAME glmark2.fbo COMMAND test_glmark2_fbo)
    set_tests_properties(glmark2.fbo PROPERTIES
        ENVIRONMENT "GLMARK2_OUT_DIR=${_glmark2_out_dir}")

    # Sprint 39 — first slice of "SC chain runs glmark2".
    # Captures the ES 2.0 draw into a .scene then forks
    # sc_pattern_runner to replay it through the cycle-accurate chain.
    if(TARGET sc_pattern_runner)
        add_executable(test_glmark2_to_sc scene_to_sc.cpp)
        target_include_directories(test_glmark2_to_sc PRIVATE
            "${CMAKE_SOURCE_DIR}/glcompat/src")
        target_link_libraries(test_glmark2_to_sc PRIVATE gpu_glmark2_runner)
        add_test(NAME glmark2.to_sc
                 COMMAND test_glmark2_to_sc $<TARGET_FILE:sc_pattern_runner>)
        set_tests_properties(glmark2.to_sc PROPERTIES
            ENVIRONMENT "GLMARK2_OUT_DIR=${_glmark2_out_dir}")
    endif()
endif()
