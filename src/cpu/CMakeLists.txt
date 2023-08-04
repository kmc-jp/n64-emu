add_library(cpu
    cpu.h cpu.cpp
    cop0.h cop0.cpp
    instruction.h
    gpr.h
    cpu_operation.h cpu_operation.cpp
    cpu_instructions.h
)
target_precompile_headers(cpu PRIVATE ${PCH_FILES})
