add_library(debugger STATIC)
target_sources(debugger PRIVATE
    debugger.cpp
)
# Do not link n64_system here (would cycle). Final n64 link pulls both.
target_link_libraries(debugger PUBLIC
    common
    cpu
    log
    memory
    mmu
    utils
    ${SDL2_LIBRARIES}
)
# mmio/rcp symbols (g_vi/g_mi/g_rsp/g_dpc) resolve via the final n64 link.
# Do not link them here: mmio/rcp <-> n64_system <-> debugger would cycle.
target_include_directories(debugger PUBLIC ${SDL2_INCLUDE_DIRS})
