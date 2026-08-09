add_library(debugger STATIC)
target_sources(debugger PRIVATE
    debugger.cpp
)
target_link_libraries(debugger PUBLIC
    common
    cpu
    log
    memory
    mmu
    utils
)
