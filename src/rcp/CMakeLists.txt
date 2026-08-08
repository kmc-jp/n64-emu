add_library(rcp STATIC)
target_sources(rcp PRIVATE
    rsp.cpp
    rsp_vector.cpp
    rsp_thread.cpp
    dpc.cpp
    vu_profile.cpp
)

if(N64_RSP_SIMD)
    target_sources(rcp PRIVATE rsp_vector_simd.cpp)
    target_link_libraries(rcp PUBLIC eve::eve)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    target_sources(rcp PRIVATE
        jit/code_cache.cpp
        jit/helpers.cpp
        jit/translate.cpp
        jit/emit_x64.cpp
        jit/vu_sse.cpp
        jit/jit.cpp
    )
    target_compile_definitions(rcp PUBLIC N64_RSP_JIT=1)
    target_link_libraries(rcp PUBLIC xbyak)
endif()

target_link_libraries(rcp PUBLIC
    common
    log
    mmio
    n64_system
    Threads::Threads
)
