add_library(mmu
    mmu.cpp mmu.h
    tlb.cpp tlb.h)
target_precompile_headers(mmu PRIVATE ${PCH_FILES})
