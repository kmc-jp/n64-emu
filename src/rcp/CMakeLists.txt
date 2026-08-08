add_library(rcp STATIC)
target_sources(rcp PRIVATE
    rsp.cpp
    rsp_vector.cpp
    dpc.cpp
)

if(N64_RSP_SIMD)
    target_sources(rcp PRIVATE rsp_vector_simd.cpp)
    target_link_libraries(rcp PUBLIC eve::eve)
endif()

target_link_libraries(rcp PUBLIC
    common
    log
    mmio
    n64_system
)
