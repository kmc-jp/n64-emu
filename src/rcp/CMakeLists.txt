add_library(rcp
    rsp.cpp rsp.h)
target_precompile_headers(rcp PRIVATE ${PCH_FILES})
