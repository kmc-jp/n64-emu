add_library(cpu STATIC)
target_sources(cpu PRIVATE
    cached_interp.cpp
    cop0.cpp
    cop1.cpp
    cpu.cpp
    cpu_instruction_impl.cpp
    fpu_instruction_impl.cpp
    idle_skip.cpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    target_sources(cpu PRIVATE
        jit/code_cache.cpp
        jit/helpers.cpp
        jit/translate.cpp
        jit/emit_x64.cpp
        jit/jit.cpp
    )
    target_compile_definitions(cpu PUBLIC N64_JIT_X64=1)
    target_link_libraries(cpu PUBLIC xbyak)
endif()

target_link_libraries(cpu PUBLIC
    common
    log
    memory
    mmu
    n64_system
    utils
)
