add_library(n64_system
    n64_system.cpp n64_system.h
    config.cpp config.h
    scheduler.cpp scheduler.h
    interrupt.cpp interrupt.h
)
target_precompile_headers(n64_system PRIVATE ${PCH_FILES})
