# Unit tests
add_executable(kamo64-test)
target_sources(kamo64-test PRIVATE
    bitfield.cpp
    stdint.cpp
    test.cpp
)
target_link_libraries(kamo64-test PUBLIC
    common
    log
)

add_test(NAME kamo64-test COMMAND kamo64-test)
