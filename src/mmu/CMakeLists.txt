add_library(mmu
    mmu.cpp mmu.h)
target_precompile_headers(mmu PRIVATE ${PCH_FILES})
