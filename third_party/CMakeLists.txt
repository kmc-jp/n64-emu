add_subdirectory(parallel-rdp-standalone)
add_subdirectory(spdlog)
add_subdirectory(xbyak)

# EVE is header-only; avoid its full project (tests/docs).
add_library(eve_lib INTERFACE)
target_include_directories(eve_lib SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/eve/include"
)
target_compile_features(eve_lib INTERFACE cxx_std_20)
add_library(eve::eve ALIAS eve_lib)
message(STATUS "${APPID_LOG_PREFIX} EVE SIMD headers: third_party/eve @ 15caeecf")

target_include_directories(spdlog SYSTEM INTERFACE
    spdlog/include
)

# Dear ImGui (SDL2 + Vulkan backends)
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)
target_link_libraries(imgui PUBLIC SDL2::SDL2 parallel-rdp)
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_IMPL_VULKAN_USE_VOLK
)
target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/parallel-rdp-standalone/volk"
)

# toml++ (header-only)
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/tomlplusplus/include"
)
target_compile_features(tomlplusplus INTERFACE cxx_std_17)

