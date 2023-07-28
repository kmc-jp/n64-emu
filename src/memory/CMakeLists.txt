add_library(memory
    memory.h memory.cpp
    bus.h bus.cpp
    pif.h pif.cpp
    rom.h rom.cpp
    ri.h ri.cpp
    memory_map.h)
target_precompile_headers(memory PRIVATE ${PCH_FILES})
