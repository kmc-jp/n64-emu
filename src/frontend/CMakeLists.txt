include_directories(${SDL2_INCLUDE_DIRS})
add_library(frontend
    frontend.h frontend.cpp)
target_precompile_headers(frontend PRIVATE ${PCH_FILES})