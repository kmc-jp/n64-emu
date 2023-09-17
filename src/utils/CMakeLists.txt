add_library(utils STATIC)
target_sources(utils PRIVATE
    utils.cpp
)
target_link_libraries(utils
    PUBLIC spdlog
    PRIVATE
    common
    cpu
)
