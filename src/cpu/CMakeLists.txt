add_library(cpu
    cpu.h cpu.cpp
    cop0.h cop0.cpp
    instruction.h
    gpr.h
    cpu_instruction.h
    instruction_execute.cpp
)
target_precompile_headers(cpu PRIVATE ${PCH_FILES})
