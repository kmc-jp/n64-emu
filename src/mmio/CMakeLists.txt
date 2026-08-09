add_library(mmio STATIC)
target_sources(mmio PRIVATE
    ai.cpp
    controller_input.cpp
    mi.cpp
    pi.cpp
    pif.cpp
    si.cpp
    vi.cpp
)
target_link_libraries(mmio PUBLIC
    audio
    common
    cpu
    log
    memory
    n64_system
    rcp
)
