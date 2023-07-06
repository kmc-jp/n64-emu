cmake_minimum_required(VERSION 3.16)
project(n64-emu)

add_executable(n64
    src/main.cpp
)

#set(Boost_DEBUG ON)
find_package(Boost)
include_directories(${Boost_INCLUDE_DIR})


# find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
# find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
#
# if(TARGET SDL2::SDL2main)
#   target_link_libraries(n64 PRIVATE SDL2::SDL2main)
# endif()

target_link_libraries(n64 LINK_PUBLIC ${Boost_LIBRARIES})
#target_link_libraries(n64 PRIVATE SDL2::SDL2)
