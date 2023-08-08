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

include_directories(
    .
    "${CMAKE_SOURCE_DIR}/third_party/spdlog/include"
    ${SDL2_INCLUDE_DIRS}
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/vulkan-headers/include"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/parallel-rdp"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/util"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/volk"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/vulkan"
)

add_executable(n64 main.cpp)

add_subdirectory(cpu)
add_subdirectory(memory)
add_subdirectory(mmio)
add_subdirectory(mmu)
add_subdirectory(rcp)
add_subdirectory(utils)
add_subdirectory(n64_system)
add_subdirectory(app)

target_link_libraries(n64 PRIVATE
    cpu memory mmio mmu rcp utils n64_system app
    parallel-rdp
    PUBLIC
    ${SDL2_LIBRARIES}
)

# target_link_libraries(n64 PRIVATE SDL2::SDL2)
target_precompile_headers(n64
    PRIVATE
    ${PCH_FILES}
)
