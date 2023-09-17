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
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imconfig.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_internal.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imstb_rectpack.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imstb_textedit.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imstb_truetype.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl2.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl2.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdlrenderer2.h
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdlrenderer2.cpp)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${SPDLOG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${PARALLEL_RDP_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
)

add_executable(n64 main.cpp)

add_library(imgui
    ${IMGUI_SOURCES})
target_link_libraries(imgui PRIVATE
    common
    PUBLIC SDL2::SDL2)

# Is this unnecessary?
# target_link_libraries(n64 PRIVATE SDL2::SDL2)
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
    parallel-rdp imgui
    common
    PUBLIC
    ${SDL2_LIBRARIES}
)

# Unit tests
add_executable(n64-test tests/test.cpp)
target_link_libraries(n64-test PRIVATE
    cpu memory mmio mmu rcp utils n64_system app
    parallel-rdp imgui
    common
    PUBLIC
    ${SDL2_LIBRARIES}
)

add_test(NAME n64-test COMMAND n64-test)
