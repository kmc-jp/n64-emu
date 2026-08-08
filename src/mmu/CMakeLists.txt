add_library(mmu STATIC)
target_sources(mmu PRIVATE
    mmu.cpp
    soft_tlb.cpp
    tlb.cpp
)
target_link_libraries(mmu PUBLIC
    common
    log
)
