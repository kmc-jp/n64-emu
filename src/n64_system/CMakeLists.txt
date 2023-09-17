add_library(n64_system
    n64_system.cpp
    config.cpp
    scheduler.cpp
    interrupt.cpp
)
target_precompile_headers(n64_system PRIVATE ${PCH_FILES})
target_link_libraries(n64_system PRIVATE
    memory
    mmio
    mmu
    rcp
)
target_link_libraries(n64_system PRIVATE
    common
)
