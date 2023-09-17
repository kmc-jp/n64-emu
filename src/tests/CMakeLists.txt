# Unit tests
add_executable(n64-test)
target_sources(n64-test PRIVATE
    test.cpp
)
target_link_libraries(n64-test PUBLIC
    common
    cpu
    n64_system
)

add_test(NAME n64-test COMMAND n64-test)
