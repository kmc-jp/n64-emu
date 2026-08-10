# CLI / windowed frontend without ImGui (tests, scripting, old workflow)
add_executable(kamo64-core)
target_sources(kamo64-core PRIVATE
    main_core.cpp
)
target_link_libraries(kamo64-core PRIVATE
    ui
    common
    n64_system
    log
)

# ImGui GUI frontend
add_executable(kamo64)
target_sources(kamo64 PRIVATE
    main.cpp
)
target_link_libraries(kamo64 PRIVATE
    ui_gui
    common
    n64_system
    log
)

add_subdirectory(audio)
add_subdirectory(cpu)
add_subdirectory(debugger)
add_subdirectory(memory)
add_subdirectory(mmio)
add_subdirectory(mmu)
add_subdirectory(n64_system)
add_subdirectory(rcp)
add_subdirectory(rdp)
add_subdirectory(tests)
add_subdirectory(utils)
add_subdirectory(video)
add_subdirectory(ui)
