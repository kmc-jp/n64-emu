add_library(mmio STATIC)
target_sources(mmio PRIVATE
    ai.cpp
    mi.cpp
    pi.cpp
    pif.cpp
    si.cpp
    vi.cpp
)
target_link_libraries(mmio PRIVATE
    common
    SDL2::SDL2
    utils
)
