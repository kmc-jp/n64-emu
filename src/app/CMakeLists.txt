include_directories(${SDL2_INCLUDE_DIRS})
add_library(app
    app.h app.cpp
)
target_precompile_headers(app PRIVATE ${PCH_FILES})
