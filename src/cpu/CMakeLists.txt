add_library(cpu
    cpu.h cpu.cpp
    cop0.h cop0.cpp
    cop1.h cop1.cpp
    instruction.cpp
    cpu_instruction_impl.cpp
    fpu_instruction_impl.cpp
)
target_precompile_headers(cpu PRIVATE ${PCH_FILES})
