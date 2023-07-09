include_directories(
    .
    cpu
    memory
    mmio
    mmu
    rsp
    utils
)

# Boost
# set(Boost_DEBUG ON)
find_package(Boost)
find_package(Threads)
include_directories(${Boost_INCLUDE_DIR})

# spdlog
include_directories("${CMAKE_SOURCE_DIR}/third_party/spdlog/include")

# SDL2
# find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
# find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
#
# if(TARGET SDL2::SDL2main)
# target_link_libraries(n64 PRIVATE SDL2::SDL2main)
# endif()
add_subdirectory(cpu)
add_subdirectory(memory)
add_subdirectory(mmio)
add_subdirectory(mmu)
add_subdirectory(rsp)
add_subdirectory(utils)

add_executable(n64 main.cpp)
target_link_libraries(n64 PUBLIC
    cpu memory mmu mmio rsp utils
    ${Boost_LIBRARIES})

# target_link_libraries(n64 PRIVATE SDL2::SDL2)