add_library(cpu
    cpu.cpp
    cop0.cpp
    cop1.cpp
    instruction.cpp
    cpu_instruction_impl.cpp
    fpu_instruction_impl.cpp
)
target_precompile_headers(cpu PRIVATE ${PCH_FILES})
target_link_libraries(cpu PRIVATE
    common
)
