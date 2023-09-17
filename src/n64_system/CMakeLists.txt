add_library(n64_system STATIC)
target_sources(n64_system PRIVATE
    config.cpp
    interrupt.cpp
    n64_system.cpp
    scheduler.cpp
)
target_link_libraries(n64_system PRIVATE
    memory
    mmio
    mmu
    rcp
)
target_link_libraries(n64_system PRIVATE
    common
    utils
)
