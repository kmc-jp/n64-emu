add_library(cpu STATIC)
target_sources(cpu PRIVATE
    cop0.cpp
    cop1.cpp
    cpu.cpp
    cpu_instruction_impl.cpp
    fpu_instruction_impl.cpp
    instruction.cpp
)
target_link_libraries(cpu PUBLIC
    common
    log
    memory
    mmu
    n64_system
    utils
)
