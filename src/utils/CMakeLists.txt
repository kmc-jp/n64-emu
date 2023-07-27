add_library(utils
    utils.cpp utils.h)
target_precompile_headers(utils PRIVATE ${PCH_FILES})
