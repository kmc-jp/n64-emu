# Shared frontend pieces (no ImGui) used by kamo64-core and kamo64.
add_library(ui STATIC)
target_sources(ui PRIVATE
    app_core.cpp
    app_paths.cpp
    audio_sdl.cpp
    config_cli.cpp
    input_sdl.cpp
    sdl_platform.cpp
    vulkan_devices.cpp
)

# ImGui GUI layer (binary: kamo64)
add_library(ui_gui STATIC)
target_sources(ui_gui PRIVATE
    app.cpp
    config_toml.cpp
    file_dialog.cpp
    gui.cpp
    imgui_layer.cpp
    recent_roms.cpp
)

set(APPID_IDENTITY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(APPID_IDENTITY_H "${APPID_IDENTITY_DIR}/app_identity.h")
set(APPID_IDENTITY_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/GenerateAboutInfo.cmake")

# Regenerate on every build so the embedded git hash stays current.
add_custom_target(generate_app_identity
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_DIR=${APPID_IDENTITY_DIR}
        -P ${APPID_IDENTITY_SCRIPT}
    BYPRODUCTS ${APPID_IDENTITY_H}
    DEPENDS ${APPID_IDENTITY_SCRIPT}
            "${CMAKE_SOURCE_DIR}/cmake/AppIdentity.cmake"
    COMMENT "Generating app_identity (name + git hash)"
    VERBATIM
)

# Ensure the header exists at configure/generate time for IDEs and first build.
if(NOT EXISTS "${APPID_IDENTITY_H}")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_DIR=${APPID_IDENTITY_DIR}
            -P ${APPID_IDENTITY_SCRIPT}
        RESULT_VARIABLE _appid_identity_gen_result
    )
    if(NOT _appid_identity_gen_result EQUAL 0)
        message(FATAL_ERROR "Failed to generate app_identity")
    endif()
endif()

target_include_directories(ui PRIVATE ${APPID_IDENTITY_DIR})
target_include_directories(ui_gui PRIVATE ${APPID_IDENTITY_DIR})
add_dependencies(ui generate_app_identity)
add_dependencies(ui_gui generate_app_identity)
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