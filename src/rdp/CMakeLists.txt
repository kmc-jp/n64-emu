add_library(rdp STATIC)
target_sources(rdp PRIVATE
    rdp_core.cpp
)
target_link_libraries(rdp PUBLIC
    common
    log
    parallel-rdp
)
