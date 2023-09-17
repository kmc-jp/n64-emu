add_library(mmio STATIC)
target_sources(mmio PRIVATE
    ai.cpp
    mi.cpp
    pi.cpp
    pif.cpp
    si.cpp
    vi.cpp
)
target_link_libraries(mmio PUBLIC
    common
    cpu
    log
    memory
    n64_system
    rcp
    SDL2::SDL2
)
