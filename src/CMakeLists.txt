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
    parallel-rdp imgui
    common
    log
    SDL2::SDL2
)

# Unit tests
add_executable(n64-test tests/test.cpp)
target_link_libraries(n64-test PRIVATE
    cpu memory mmio mmu rcp utils n64_system app
    parallel-rdp imgui
    common
    log
    SDL2::SDL2
)

add_test(NAME n64-test COMMAND n64-test)
