add_library(memory STATIC)
target_sources(memory PRIVATE
    bus.cpp
    memory.cpp
    ri.cpp
    rom.cpp
)
target_link_libraries(memory PUBLIC
    common
    log
    mmio
    utils
)
