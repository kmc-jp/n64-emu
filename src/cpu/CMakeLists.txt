add_library(cpu
    cpu.h cpu.cpp
    cop0.h cop0.cpp
    cop1.h cop1.cpp
    instruction.h
    cpu_instruction_impl.h
    fpu_instruction_impl.h
)
target_precompile_headers(cpu PRIVATE ${PCH_FILES})
