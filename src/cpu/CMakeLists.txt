add_library(cpu
    cpu.h cpu.cpp
    cop0.h cop0.cpp
    instruction.h
    gpr.h
    instruction_execute.cpp
    cpu_instruction_impl.h
    fpu_instruction_impl.h
)
target_precompile_headers(cpu PRIVATE ${PCH_FILES})
