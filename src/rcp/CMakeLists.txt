add_library(rcp
    rsp.cpp)
target_precompile_headers(rcp PRIVATE ${PCH_FILES})
target_link_libraries(rcp PRIVATE
    common
)
