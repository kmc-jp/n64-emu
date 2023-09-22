add_library(rcp STATIC)
target_sources(rcp PRIVATE
    rsp.cpp
)
target_link_libraries(rcp PUBLIC
    common
    log
    mmio
    n64_system
)
