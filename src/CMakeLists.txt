set(PCH_FILES
    <cassert>
    <cstdint>
    <fstream>
    <functional>
    <iostream>
    <queue>
    <source_location>
    <span>
    <string>
    <utility>
    <array>
    <vector>
    <optional>
    <spdlog/spdlog.h>
    <spdlog/sinks/basic_file_sink.h>
)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

include_directories(
    .
    cpu
    memory
    mmio
    mmu
    rcp
    utils
    n64_system
    frontend
    "${CMAKE_SOURCE_DIR}/third_party/spdlog/include"
)

add_executable(n64 main.cpp)

add_subdirectory(cpu)
add_subdirectory(memory)
add_subdirectory(mmio)
add_subdirectory(mmu)
add_subdirectory(rcp)
add_subdirectory(utils)
add_subdirectory(n64_system)
add_subdirectory(frontend)

target_link_libraries(n64 PRIVATE
    cpu memory mmio mmu rcp utils n64_system frontend
    PUBLIC
    ${SDL2_LIBRARIES}
)

# target_link_libraries(n64 PRIVATE SDL2::SDL2)
target_precompile_headers(n64
    PRIVATE
    ${PCH_FILES}
)
