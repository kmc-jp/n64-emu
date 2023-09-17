add_library(memory
    memory.cpp
    bus.cpp
    rom.cpp
    ri.cpp
)
target_precompile_headers(memory PRIVATE ${PCH_FILES})
