cmake_minimum_required(VERSION 3.16)
project(n64-emu)
set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/O2")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-O0")
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
