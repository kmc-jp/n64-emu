add_subdirectory(parallel-rdp-standalone)
add_subdirectory(spdlog)
add_subdirectory(xbyak)

# EVE is header-only; avoid its full project (tests/docs).
add_library(eve_lib INTERFACE)
target_include_directories(eve_lib SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/eve/include"
)
target_compile_features(eve_lib INTERFACE cxx_std_20)
add_library(eve::eve ALIAS eve_lib)
message(STATUS "[n64-emu] EVE SIMD headers: third_party/eve @ 15caeecf")

target_include_directories(spdlog SYSTEM INTERFACE
    spdlog/include
)
