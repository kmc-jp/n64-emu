add_library(app STATIC)
target_sources(app PRIVATE
    app.cpp
    parallel_rdp_wrapper.cpp
)
target_link_libraries(app PRIVATE
    common
    n64_system
    parallel-rdp
    SDL2::SDL2
    utils
)
