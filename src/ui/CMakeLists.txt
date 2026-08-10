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
    file_dialog.cpp
    gui.cpp
    imgui_layer.cpp
    recent_roms.cpp
)

set(N64_ABOUT_INFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(N64_ABOUT_INFO_H "${N64_ABOUT_INFO_DIR}/about_info.h")
set(N64_ABOUT_INFO_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/GenerateAboutInfo.cmake")

# Regenerate on every build so the embedded git hash stays current.
add_custom_target(generate_about_info
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_DIR=${N64_ABOUT_INFO_DIR}
        -P ${N64_ABOUT_INFO_SCRIPT}
    BYPRODUCTS ${N64_ABOUT_INFO_H}
    DEPENDS ${N64_ABOUT_INFO_SCRIPT}
    COMMENT "Generating about_info (git hash)"
    VERBATIM
)

# Ensure the header exists at configure/generate time for IDEs and first build.
if(NOT EXISTS "${N64_ABOUT_INFO_H}")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_DIR=${N64_ABOUT_INFO_DIR}
            -P ${N64_ABOUT_INFO_SCRIPT}
        RESULT_VARIABLE _n64_about_gen_result
    )
    if(NOT _n64_about_gen_result EQUAL 0)
        message(FATAL_ERROR "Failed to generate about_info")
    endif()
endif()

target_include_directories(ui_gui PRIVATE ${N64_ABOUT_INFO_DIR})
add_dependencies(ui_gui generate_about_info)
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
)
if(WIN32)
    target_link_libraries(ui_gui PUBLIC comdlg32 shell32)
endif()