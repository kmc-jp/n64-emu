add_library(utils STATIC)
target_sources(utils PRIVATE
    utils.cpp
)
target_link_libraries(utils PUBLIC
    common
    log
)

add_library(log STATIC)
target_sources(log PRIVATE
    log.cpp
)
target_link_libraries(log PUBLIC
    common
    cpu
    spdlog
)
target_precompile_headers(log PUBLIC
    <spdlog/spdlog.h>
)
