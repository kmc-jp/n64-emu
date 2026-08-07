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
target_include_directories(debugger PUBLIC ${SDL2_INCLUDE_DIRS})
