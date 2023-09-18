# Unit tests
add_executable(n64-test)
target_sources(n64-test PRIVATE
    bitfield.cpp
    test.cpp
)
target_link_libraries(n64-test PUBLIC
    common
    log
)

add_test(NAME n64-test COMMAND n64-test)
