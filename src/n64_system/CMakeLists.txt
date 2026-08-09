add_library(n64_system STATIC)
target_sources(n64_system PRIVATE
    interrupt.cpp
    machine_advance.cpp
    n64_system.cpp
    scheduler.cpp
)
target_link_libraries(n64_system PUBLIC
    common
    cpu
    debugger
    log
    memory
    mmio
    mmu
    rcp
)
