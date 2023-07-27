add_library(memory
    memory.h memory.cpp
    bus.h bus.cpp
    pif.h
    rom.h rom.cpp
    ri.h ri.cpp)
target_precompile_headers(memory PRIVATE ${PCH_FILES})
