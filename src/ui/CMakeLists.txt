# Shared frontend pieces (no ImGui) used by n64-core and n64.
add_library(ui STATIC)
target_sources(ui PRIVATE
    app_core.cpp
    audio_sdl.cpp
    config_cli.cpp
    input_sdl.cpp
    sdl_platform.cpp
    vulkan_devices.cpp
)

# ImGui GUI layer (binary: n64)
add_library(ui_gui STATIC)
target_sources(ui_gui PRIVATE
    app.cpp
    config_toml.cpp
    gui.cpp
    imgui_layer.cpp
    recent_roms.cpp
    win32_file_dialog.cpp
)

target_link_libraries(ui PUBLIC
    audio
    common
    log
    memory
    mmio
    n64_system
    video
    SDL2::SDL2
    parallel-rdp
)

target_link_libraries(ui_gui PUBLIC
    ui
    imgui
    tomlplusplus
    comdlg32
)
