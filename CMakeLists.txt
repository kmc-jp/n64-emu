cmake_minimum_required(VERSION 3.16)
project(n64-emu)
set (CMAKE_CXX_STANDARD 20)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
