add_library(mmio
    pi.h pi.cpp si.h si.cpp)
target_precompile_headers(mmio PRIVATE ${PCH_FILES})
