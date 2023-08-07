include_directories(${SDL2_INCLUDE_DIRS})
add_library(frontend
    frontend.h frontend.cpp
    renderer.cpp renderer.h
    parallel_rdp_wrapper.cpp
    parallel_rdp_wrapper.h
    parallel_rdp_wrapper_sdl.h
)
target_precompile_headers(frontend PRIVATE ${PCH_FILES})
