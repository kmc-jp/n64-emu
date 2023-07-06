cmake_minimum_required(VERSION 3.16)
project(n64-emu)

FIND_PACKAGE( Boost 1.40 COMPONENTS program_options REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

add_executable(n64
    src/main.cpp
)
TARGET_LINK_LIBRARIES(n64 LINK_PUBLIC ${Boost_LIBRARIES} )
