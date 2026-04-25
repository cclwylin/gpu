# SystemC TLM-LT skeleton (Sprint 5).
# Optional — requires SystemC installation. Disabled by default; enable with
#   -DGPU_BUILD_SYSTEMC=ON
# CMake hint: SystemC_DIR=/path/to/lib/cmake/SystemCLanguage

option(GPU_BUILD_SYSTEMC "Build SystemC TLM model (requires SystemC)" OFF)
if(NOT GPU_BUILD_SYSTEMC)
    return()
endif()

find_package(SystemCLanguage CONFIG QUIET)
if(NOT SystemCLanguage_FOUND)
    # Common install path on macOS / Ubuntu
    foreach(hint
            /usr/local/systemc-2.3.4/lib/cmake/SystemCLanguage
            /usr/local/systemc-2.3.3/lib/cmake/SystemCLanguage
            $ENV{SYSTEMC_HOME}/lib/cmake/SystemCLanguage)
        if(EXISTS "${hint}/SystemCLanguageConfig.cmake")
            set(SystemCLanguage_DIR "${hint}" CACHE PATH "" FORCE)
            find_package(SystemCLanguage CONFIG REQUIRED)
            break()
        endif()
    endforeach()
endif()
if(NOT SystemCLanguage_FOUND)
    message(WARNING "SystemC not found; skipping systemc/")
    return()
endif()

message(STATUS "Found SystemC: ${SystemCLanguage_DIR}")

add_library(gpu_systemc STATIC
    blocks/commandprocessor/src/commandprocessor.cpp
    blocks/commandprocessor/src/commandprocessor_ca.cpp
    blocks/shadercore/src/shadercore.cpp
    blocks/vertexfetch/src/vertexfetch.cpp
    blocks/primitiveassembly/src/primitiveassembly.cpp
    blocks/rasterizer/src/rasterizer.cpp
    top/src/gpu_top.cpp
)
target_include_directories(gpu_systemc PUBLIC
    common/include
    blocks/commandprocessor/include
    blocks/shadercore/include
    blocks/vertexfetch/include
    blocks/primitiveassembly/include
    blocks/rasterizer/include
    top/include)
target_link_libraries(gpu_systemc PUBLIC SystemC::systemc gpu_compiler)
# SystemC lib is a dylib on macOS; CMake may need the rpath.
set_target_properties(gpu_systemc PROPERTIES
    INSTALL_RPATH "/usr/local/systemc-2.3.4/lib"
    BUILD_RPATH   "/usr/local/systemc-2.3.4/lib")

if(GPU_BUILD_TESTS)
    # macOS-installed SystemC is typically built with AppleClang/libc++; if
    # we're compiling with GNU/libstdc++ they ABI-mismatch and the executable
    # fails to link. The library above still compiles cleanly; CI runs the
    # test inside the Docker image where both halves share libstdc++.
    set(_skip_tlm_test FALSE)
    if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(_skip_tlm_test TRUE)
        message(STATUS "Skipping systemc.tlm_hello on macOS+GCC (libc++/libstdc++ ABI mismatch); use Docker.")
    endif()
    if(NOT _skip_tlm_test)
        add_executable(test_tlm_hello tb/test_tlm_hello.cpp)
        target_link_libraries(test_tlm_hello PRIVATE gpu_systemc)
        set_target_properties(test_tlm_hello PROPERTIES
            INSTALL_RPATH "/usr/local/systemc-2.3.4/lib"
            BUILD_RPATH   "/usr/local/systemc-2.3.4/lib")
        add_test(NAME systemc.tlm_hello COMMAND test_tlm_hello)

        add_executable(test_commandprocessor_ca tb/test_commandprocessor_ca.cpp)
        target_link_libraries(test_commandprocessor_ca PRIVATE gpu_systemc)
        set_target_properties(test_commandprocessor_ca PROPERTIES
            INSTALL_RPATH "/usr/local/systemc-2.3.4/lib"
            BUILD_RPATH   "/usr/local/systemc-2.3.4/lib")
        add_test(NAME systemc.cp_ca COMMAND test_commandprocessor_ca)
    endif()
endif()
