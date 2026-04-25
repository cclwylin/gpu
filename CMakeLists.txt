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

option(GPU_BUILD_TESTS "Build unit tests" ON)

enable_testing()

add_subdirectory(sw_ref)

if(GPU_BUILD_TESTS)
    add_subdirectory(sw_ref/tests)
endif()
