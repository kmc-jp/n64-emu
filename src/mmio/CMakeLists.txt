add_library(mmio
    pi.cpp
    si.cpp
    pif.cpp
    mi.cpp
    ai.cpp
    vi.cpp
)
target_precompile_headers(mmio PRIVATE ${PCH_FILES})
