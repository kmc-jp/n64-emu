cmake_minimum_required(VERSION 3.16)
project(n64-emu)
enable_language(CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

find_package(SDL2 REQUIRED)

add_library(common INTERFACE)
target_compile_features(common INTERFACE cxx_std_20)

# Copy of https://fuchsia.googlesource.com/third_party/safeside/+/refs/heads/upstream/analysis-py/demos/CMakeLists.txt
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # Remove a few optimization-related MSVC flags that CMake includes in Debug
    # builds by default: https://git.io/Jv0F0
    # Remove `/Od`, which disables all optimizations.
    # https://docs.microsoft.com/en-us/cpp/build/reference/od-disable-debug?view=vs-2019
    string(REPLACE "/Od" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

    # Remove `/Ob0`, which disables *all* function inlining -- even for functions
    # marked `__forceinline`.
    # https://docs.microsoft.com/en-us/cpp/build/reference/ob-inline-function-expansion?view=vs-2019
    string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

    # Remove `/RTC1`, which enables run-time error checks. These checks are
    # incompatible with optimizations and the build fails if both are enabled.
    # https://docs.microsoft.com/en-us/cpp/build/reference/rtc-run-time-error-checks?view=vs-2019
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

target_compile_options(common INTERFACE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Z7 /O2>
)

target_include_directories(common INTERFACE include)
target_precompile_headers(common INTERFACE
    <array>
    <cassert>
    <cstdint>
    <fstream>
    <functional>
    <iostream>
    <optional>
    <queue>
    <source_location>
    <span>
    <string>
    <utility>
    <vector>
)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(third_party)
