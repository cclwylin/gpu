# glcompat — OpenGL 1.x → sw_ref/SC compatibility shim.
#
# Sprint 36 — provides a libgl_compat.a that satisfies the gl*/glu*/glut*
# symbols used by tests/examples/*.c, redirecting them to gpu::pipeline.

add_library(gpu_glcompat STATIC
    src/glcompat_state.cpp
    src/glcompat_immediate.cpp
    src/glcompat_render.cpp
    src/glcompat_glu.cpp
    src/glcompat_glut.cpp
    src/glcompat_glut_geom.cpp
)
target_include_directories(gpu_glcompat PUBLIC include)
target_link_libraries(gpu_glcompat PUBLIC gpu_sw_ref gpu_compiler)
set_target_properties(gpu_glcompat PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Build each .c under tests/examples/ as an offscreen-rendering binary.
# The example writes a PPM to $GLCOMPAT_OUT (default ./out.ppm) on exit.
option(GPU_BUILD_GL_EXAMPLES "Build tests/examples/*.c as glcompat binaries" ON)
if(GPU_BUILD_GL_EXAMPLES)
    file(GLOB GL_EXAMPLE_C "${CMAKE_SOURCE_DIR}/tests/examples/*.c")
    # Helper-only files (no main()) — included by other examples, not
    # built standalone.
    set(_glex_skip
        "${CMAKE_SOURCE_DIR}/tests/examples/logo.c"
        "${CMAKE_SOURCE_DIR}/tests/examples/mjkimage.c"
        "${CMAKE_SOURCE_DIR}/tests/examples/trackball.c")
    foreach(ex ${GL_EXAMPLE_C})
        if(ex IN_LIST _glex_skip)
            continue()
        endif()
        get_filename_component(name ${ex} NAME_WE)
        # Compile each .c as a stand-alone binary.
        add_executable(glex_${name} EXCLUDE_FROM_ALL ${ex})
        target_link_libraries(glex_${name} PRIVATE gpu_glcompat)
        # Hide warnings from the legacy K&R-ish C in the corpus — these
        # are not our code to clean up.
        target_compile_options(glex_${name} PRIVATE
            -w
            $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
        # The corpus often #includes <GL/glut.h> *before* declaring main —
        # nothing to do, our headers cover it. Some files use trackball.h
        # / dynamic.h locally — search the example dir as well.
        target_include_directories(glex_${name} PRIVATE
            "${CMAKE_SOURCE_DIR}/tests/examples")
    endforeach()

    # Convenience aggregate: `cmake --build build --target gl_examples_smoke`
    # builds the few examples we expect to render correctly.
    add_custom_target(gl_examples_smoke
        DEPENDS glex_simple)
endif()
