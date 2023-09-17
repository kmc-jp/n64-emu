add_library(mmu
    mmu.cpp
    tlb.cpp)
target_precompile_headers(mmu PRIVATE ${PCH_FILES})
target_link_libraries(mmu PRIVATE
    common
)
