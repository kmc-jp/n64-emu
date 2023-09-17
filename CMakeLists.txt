cmake_minimum_required(VERSION 3.16)
project(n64-emu)
enable_language(CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

find_package(SDL2 REQUIRED)

add_library(common INTERFACE)
target_compile_features(common INTERFACE cxx_std_20)
target_compile_options(common INTERFACE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/O2 /Z7>
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
