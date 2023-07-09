cmake_minimum_required(VERSION 3.16)
project(n64-emu)

set(SRC_FILES
    src/main.cpp
    src/cpu/cpu.cpp
    src/memory/memory.cpp
    src/rsp/rsp.cpp
    src/utils/utils.cpp
)

add_executable(n64 ${SRC_FILES})

#set(Boost_DEBUG ON)
find_package(Boost)
find_package(Threads)
include_directories(${Boost_INCLUDE_DIR})

include_directories("${CMAKE_SOURCE_DIR}/third_party/spdlog/include")

# find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
# find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
#
# if(TARGET SDL2::SDL2main)
#   target_link_libraries(n64 PRIVATE SDL2::SDL2main)
# endif()

target_link_libraries(n64 ${Boost_LIBRARIES})
#target_link_libraries(n64 PRIVATE SDL2::SDL2)
