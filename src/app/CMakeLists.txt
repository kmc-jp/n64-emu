include_directories(${SDL2_INCLUDE_DIRS})
add_library(app
    app.cpp
    parallel_rdp_wrapper.cpp
)
target_precompile_headers(app PRIVATE ${PCH_FILES})
target_link_libraries(app PRIVATE
    n64_system
)
