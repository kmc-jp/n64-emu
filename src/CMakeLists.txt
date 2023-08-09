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

set(SPDLOG_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/third_party/spdlog/include")

set(PARALLEL_RDP_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/vulkan-headers/include"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/parallel-rdp"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/util"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/volk"
    "${CMAKE_SOURCE_DIR}/third_party/parallel-rdp-standalone/vulkan")

set(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
file(GLOB IMGUI_SOURCES ${IMGUI_INCLUDE_DIR}/*.cpp)
file(GLOB IMGUI_HEADERS ${IMGUI_INCLUDE_DIR}/*.h)

include_directories(
    .
    ${SPDLOG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${PARALLEL_RDP_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
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
    ${IMGUI_SOURCCES}
)

# target_link_libraries(n64 PRIVATE SDL2::SDL2)
target_precompile_headers(n64
    PRIVATE
    ${PCH_FILES}
)
