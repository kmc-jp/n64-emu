add_library(utils
    utils.cpp)
target_precompile_headers(utils PRIVATE ${PCH_FILES})
target_link_libraries(utils PRIVATE
    common
    cpu
)
