add_library(rcp STATIC)
target_sources(rcp PRIVATE
    rsp.cpp
)
target_link_libraries(rcp PRIVATE
    common
    utils
)
