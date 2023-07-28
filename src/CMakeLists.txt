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
    <vector>
    <spdlog/spdlog.h>
    <spdlog/sinks/basic_file_sink.h>)

include_directories(
    .
    cpu
    memory
    mmio
    mmu
    rsp
    utils
    n64_system
    "${CMAKE_SOURCE_DIR}/third_party/spdlog/include"
)

add_executable(n64 main.cpp)

add_subdirectory(cpu)
add_subdirectory(memory)
add_subdirectory(mmio)
add_subdirectory(mmu)
add_subdirectory(rsp)
add_subdirectory(utils)
add_subdirectory(n64_system)

target_link_libraries(n64 PUBLIC
    cpu memory mmio mmu rsp utils n64_system)

# target_link_libraries(n64 PRIVATE SDL2::SDL2)
target_precompile_headers(n64
    PRIVATE
    ${PCH_FILES}
)
