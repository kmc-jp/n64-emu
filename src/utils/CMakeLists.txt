add_library(utils STATIC)
target_sources(utils PRIVATE
    byte_array.cpp
)
target_link_libraries(utils PUBLIC
    common
)

add_library(log STATIC)
target_sources(log PRIVATE
    log.cpp
    utils.cpp
)
target_link_libraries(log PUBLIC
    common
    cpu
    spdlog
)
target_precompile_headers(log PUBLIC
    <spdlog/spdlog.h>
)
