add_library(audio STATIC)
target_sources(audio PRIVATE
    audio.cpp
)
target_link_libraries(audio PUBLIC
    common
    log
)
