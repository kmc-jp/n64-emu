add_executable(n64)
target_sources(n64 PRIVATE
    main.cpp
)
target_link_libraries(n64 PRIVATE
    app
    common
    n64_system
    log
)

add_subdirectory(app)
add_subdirectory(cpu)
add_subdirectory(memory)
add_subdirectory(mmio)
add_subdirectory(mmu)
add_subdirectory(n64_system)
add_subdirectory(rcp)
add_subdirectory(tests)
add_subdirectory(utils)
