# =============================================================================
# GPU project — top-level CMake
# =============================================================================
# Phase 0/1: only sw_ref + tests build. Other components added per phase.
# =============================================================================

cmake_minimum_required(VERSION 3.20)
project(gpu LANGUAGES CXX C VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Common warning / sanitiser knobs
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wnon-virtual-dtor
    -Wno-unused-parameter
)

# -----------------------------------------------------------------------------
# macOS libc++ workaround
# -----------------------------------------------------------------------------
# Apple's Command Line Tools sometimes ship with /Library/Developer/
# CommandLineTools/usr/include/c++/v1/ in a half-installed state (only ~12
# headers instead of the full ~190). When that happens, even <cstdint> won't
# resolve, even though the full tree exists under the active SDK. We detect
# the broken state and inject the SDK's libc++ via -isystem so the build
# survives on a stale CLT install. Fully reinstalling CLT remains the proper
# fix; this just keeps the project building until then. No-op on Linux/CI.
if(APPLE)
    execute_process(COMMAND xcrun --show-sdk-path
                    OUTPUT_VARIABLE GPU_MAC_SDK_PATH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(GPU_MAC_SDK_PATH AND EXISTS "${GPU_MAC_SDK_PATH}/usr/include/c++/v1/cstdint")
        # Always inject — harmless on a healthy CLT (same headers), critical on
        # a broken one. Needs to come before any other include path.
        add_compile_options(-isystem "${GPU_MAC_SDK_PATH}/usr/include/c++/v1")
        message(STATUS "macOS libc++ shim: -isystem ${GPU_MAC_SDK_PATH}/usr/include/c++/v1")
    endif()
endif()

option(GPU_BUILD_TESTS "Build unit tests" ON)
# Sprint 42 — opt-in gate that registers the `vkglcts.*` ctest entries which
# drive Khronos VK-GL-CTS through our gpu_glcompat shim. Default OFF because
# the CTS binary lives outside this build dir (build_vkglcts/) and must be
# built first via `tools/build_vkglcts.sh`. When the gate is OFF the `vkglcts`
# namespace simply doesn't appear in `ctest -N` output.
option(GPU_BUILD_VKGLCTS "Register VK-GL-CTS ctest entries (requires tools/build_vkglcts.sh)" OFF)

enable_testing()

add_subdirectory(compiler)
add_subdirectory(compiler/glslang)
add_subdirectory(sw_ref)
add_subdirectory(systemc)
add_subdirectory(glcompat)

if(GPU_BUILD_TESTS)
    add_subdirectory(compiler/tests)
    add_subdirectory(sw_ref/tests)
    add_subdirectory(tests/conformance)
    add_subdirectory(tests/glmark2_runner)
endif()
